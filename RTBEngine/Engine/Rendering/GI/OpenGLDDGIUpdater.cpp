#include "OpenGLDDGIUpdater.h"
#include "../RHI/RenderDevice.h"
#include "../RHI/GraphicsAPI.h"
#include "../Lighting/LightingUBO.h"
#include "../Mesh.h"
#include "../../Core/ResourceManager.h"
#include "../../Core/Logger.h"
#include "../../Scene/Scene.h"
#include "../../Scene/MeshRenderer.h"
#include "../../Scene/StaticFlagsUtil.h"
#include "../../Scene/GameObject.h"
#include "../../Animation/Animator.h"
#include "../../Math/Vectors/Vector4.h"
#include "GiTypes.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdint>

namespace RTBEngine {
    namespace Rendering {
        namespace GI {

            namespace {
                // Soft-RT budget: OpenGL has no TLAS/rayQuery, so keep this tiny vs Vulkan.
                constexpr int kMaxSoftwareTriangles = 4096;
                constexpr int kGlRaysPerProbe = 24;
                constexpr int kGlMaxProbesPerFrame = 32;

                std::string LoadShaderSource(const char* path)
                {
                    const std::string resolved = Core::ResourceManager::GetInstance().ResolvePathForRead(path);
                    std::ifstream file(resolved);
                    if (!file) return {};
                    std::ostringstream ss;
                    ss << file.rdbuf();
                    std::string src = ss.str();
                    const std::string includeToken = "#include \"ddgi_common.glsl\"";
                    const auto pos = src.find(includeToken);
                    if (pos != std::string::npos) {
                        const std::string commonPath = Core::ResourceManager::GetInstance()
                            .ResolvePathForRead("Default/Shaders/ddgi_common.glsl");
                        std::ifstream commonFile(commonPath);
                        if (commonFile) {
                            std::ostringstream commonSs;
                            commonSs << commonFile.rdbuf();
                            std::string common = commonSs.str();
                            // Strip a leading #version from the include if present.
                            const auto versionPos = common.find("#version");
                            if (versionPos != std::string::npos) {
                                const auto lineEnd = common.find('\n', versionPos);
                                common.erase(versionPos,
                                    (lineEnd == std::string::npos) ? common.size() - versionPos : lineEnd - versionPos + 1);
                            }
                            src.replace(pos, includeToken.size(), common);
                        }
                    }
                    return src;
                }

                Math::Vector3 TransformPoint(const Math::Matrix4& m, const Math::Vector3& p)
                {
                    const Math::Vector4 r = m * Math::Vector4(p.x, p.y, p.z, 1.0f);
                    if (std::abs(r.w) > 1e-8f) {
                        return Math::Vector3(r.x / r.w, r.y / r.w, r.z / r.w);
                    }
                    return Math::Vector3(r.x, r.y, r.z);
                }

                void HashMix(std::uint64_t& h, std::uint64_t v)
                {
                    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                }

                std::uint64_t HashFloatBits(float f)
                {
                    std::uint32_t bits = 0;
                    std::memcpy(&bits, &f, sizeof(bits));
                    return bits;
                }
            }

            bool OpenGLDDGIUpdater::Initialize()
            {
                auto& device = RHI::RenderDevice::Get();
                const auto caps = device.GetGiCapabilities();
                if (!caps.computeShaders || !caps.storageImages || !caps.storageBuffers) {
                    RTB_WARN("OpenGLDDGIUpdater: compute/storage not available");
                    return false;
                }

                const std::string source = LoadShaderSource("Default/Shaders/ddgi_trace_gl.comp");
                if (source.empty()) {
                    RTB_WARN("OpenGLDDGIUpdater: compute shader missing");
                    return false;
                }

                computeProgram = device.CreateComputeProgram(source);
                if (computeProgram == RHI::kInvalidGpuId) {
                    RTB_WARN("OpenGLDDGIUpdater: compute program failed");
                    return false;
                }

                paramsUBO = device.CreateBuffer();
                EnsureTriangleBuffer(4 + static_cast<std::size_t>(kMaxSoftwareTriangles) * 12);
                ready = triangleSSBO != RHI::kInvalidGpuId;
                if (ready) {
                    RTB_INFO("OpenGLDDGIUpdater: software-RT DDGI ready (budget "
                        + std::to_string(kGlMaxProbesPerFrame) + " probes × "
                        + std::to_string(kGlRaysPerProbe) + " rays, max "
                        + std::to_string(kMaxSoftwareTriangles) + " tris)");
                }
                return ready;
            }

            void OpenGLDDGIUpdater::Shutdown()
            {
                if (!RHI::RenderDevice::HasDevice()) {
                    ready = false;
                    return;
                }
                auto& device = RHI::RenderDevice::Get();
                if (computeProgram != RHI::kInvalidGpuId) {
                    device.DestroyComputeProgram(computeProgram);
                    computeProgram = RHI::kInvalidGpuId;
                }
                if (paramsUBO != RHI::kInvalidGpuId) {
                    device.DestroyBuffer(paramsUBO);
                    paramsUBO = RHI::kInvalidGpuId;
                }
                if (triangleSSBO != RHI::kInvalidGpuId) {
                    device.DestroyStorageBuffer(triangleSSBO);
                    triangleSSBO = RHI::kInvalidGpuId;
                }
                triangleBufferCapacityFloats = 0;
                cachedSceneSignature = 0;
                hasCachedScene = false;
                ready = false;
            }

            void OpenGLDDGIUpdater::EnsureTriangleBuffer(std::size_t floatCount)
            {
                auto& device = RHI::RenderDevice::Get();
                if (triangleSSBO != RHI::kInvalidGpuId && triangleBufferCapacityFloats >= floatCount) {
                    return;
                }
                if (triangleSSBO != RHI::kInvalidGpuId) {
                    device.DestroyStorageBuffer(triangleSSBO);
                    triangleSSBO = RHI::kInvalidGpuId;
                }
                triangleSSBO = device.CreateStorageBuffer(floatCount * sizeof(float));
                triangleBufferCapacityFloats = (triangleSSBO != RHI::kInvalidGpuId) ? floatCount : 0;
                hasCachedScene = false;
            }

            bool OpenGLDDGIUpdater::UploadSceneTriangles(Scene::Scene* scene)
            {
                // Signature mirrors Vulkan AS cache: skip rebuild when meshes/transforms stable.
                std::uint64_t signature = 0;
                if (scene) {
                    for (Scene::MeshRenderer* renderer : scene->GetCachedMeshRenderers()) {
                        if (!Scene::RendererContributesGI(renderer)) continue;

                        HashMix(signature, reinterpret_cast<std::uintptr_t>(renderer));
                        Scene::GameObject* owner = renderer->GetOwner();
                        if (!owner) continue;
                        const Math::Matrix4& world = owner->GetWorldMatrix();
                        for (int i = 0; i < 16; ++i) {
                            HashMix(signature, HashFloatBits(world.m[i]));
                        }
                        if (renderer->IsMultiMesh()) {
                            for (Mesh* mesh : renderer->GetMeshes()) {
                                HashMix(signature, reinterpret_cast<std::uintptr_t>(mesh));
                            }
                        } else {
                            HashMix(signature, reinterpret_cast<std::uintptr_t>(renderer->GetMesh()));
                        }
                    }
                }

                if (hasCachedScene && signature == cachedSceneSignature && triangleSSBO != RHI::kInvalidGpuId) {
                    return false;
                }

                std::vector<float> payload;
                payload.resize(4, 0.0f); // header: count + 3 pads

                if (!scene) {
                    int zero = 0;
                    std::memcpy(payload.data(), &zero, sizeof(int));
                    EnsureTriangleBuffer(payload.size());
                    RHI::RenderDevice::Get().UpdateStorageBuffer(triangleSSBO, payload.data(),
                        payload.size() * sizeof(float), 0);
                    cachedSceneSignature = signature;
                    hasCachedScene = true;
                    return true;
                }

                int triCount = 0;
                for (Scene::MeshRenderer* renderer : scene->GetCachedMeshRenderers()) {
                    if (!Scene::RendererContributesGI(renderer)) continue;

                    Scene::GameObject* owner = renderer->GetOwner();
                    if (!owner) continue;

                    const auto appendMesh = [&](Mesh* mesh) {
                        if (!mesh || triCount >= kMaxSoftwareTriangles) return;
                        const auto& verts = mesh->GetCpuVertices();
                        const auto& indices = mesh->GetCpuIndices();
                        if (verts.empty() || indices.size() < 3) return;

                        const Math::Matrix4& world = owner->GetWorldMatrix();
                        const std::size_t triTotal = indices.size() / 3;
                        // Decimate large meshes so we stay under the soft-RT budget.
                        const std::size_t stride = std::max<std::size_t>(1, triTotal / 1024);

                        for (std::size_t t = 0; t + 2 < indices.size() && triCount < kMaxSoftwareTriangles; t += 3 * stride) {
                            const unsigned int i0 = indices[t];
                            const unsigned int i1 = indices[t + 1];
                            const unsigned int i2 = indices[t + 2];
                            if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size()) continue;

                            const Math::Vector3 w0 = TransformPoint(world, verts[i0].position);
                            const Math::Vector3 w1 = TransformPoint(world, verts[i1].position);
                            const Math::Vector3 w2 = TransformPoint(world, verts[i2].position);

                            payload.push_back(w0.x); payload.push_back(w0.y); payload.push_back(w0.z); payload.push_back(0.0f);
                            payload.push_back(w1.x); payload.push_back(w1.y); payload.push_back(w1.z); payload.push_back(0.0f);
                            payload.push_back(w2.x); payload.push_back(w2.y); payload.push_back(w2.z); payload.push_back(0.0f);
                            ++triCount;
                        }
                    };

                    if (renderer->IsMultiMesh()) {
                        for (Mesh* mesh : renderer->GetMeshes()) {
                            appendMesh(mesh);
                        }
                    } else {
                        appendMesh(renderer->GetMesh());
                    }
                }

                std::memcpy(payload.data(), &triCount, sizeof(int));
                EnsureTriangleBuffer(payload.size());
                if (triangleSSBO == RHI::kInvalidGpuId) return false;
                RHI::RenderDevice::Get().UpdateStorageBuffer(triangleSSBO, payload.data(),
                    payload.size() * sizeof(float), 0);

                cachedSceneSignature = signature;
                hasCachedScene = true;
                return true;
            }

            void OpenGLDDGIUpdater::Update(DDGIVolume& volume, RayTracingScene& /*rtScene*/, Scene::Scene* scene, int frameIndex)
            {
                if (!ready) return;

                const DDGISettings& settings = volume.GetSettings();
                if (!settings.enabled) return;

                auto& device = RHI::RenderDevice::Get();
                volume.EnsureGpuResources(device);
                volume.UploadUBO(device);
                UploadSceneTriangles(scene);

                struct Params {
                    float origin[3];
                    float hysteresis;
                    float spacing[3];
                    float normalBias;
                    int gridDims[3];
                    int probeCount;
                    int frameIndex;
                    int raysPerProbe;
                    int probesThisFrame;
                    float probeRadius;
                    float pad;
                } params{};

                params.origin[0] = settings.origin.x;
                params.origin[1] = settings.origin.y;
                params.origin[2] = settings.origin.z;
                params.hysteresis = settings.hysteresis;
                params.spacing[0] = settings.extent.x / std::max(settings.gridX, 1);
                params.spacing[1] = settings.extent.y / std::max(settings.gridY, 1);
                params.spacing[2] = settings.extent.z / std::max(settings.gridZ, 1);
                params.normalBias = settings.normalBias;
                params.gridDims[0] = settings.gridX;
                params.gridDims[1] = settings.gridY;
                params.gridDims[2] = settings.gridZ;
                params.probeCount = DDGIProbeCount(settings);
                params.frameIndex = frameIndex;
                params.raysPerProbe = kGlRaysPerProbe;
                params.probesThisFrame = std::min(kGlMaxProbesPerFrame, params.probeCount);
                params.probeRadius = settings.probeRadius;

                device.SetUniformBufferData(paramsUBO, &params, sizeof(params), RHI::BufferUsage::Dynamic);
                device.BindComputeProgram(computeProgram);
                device.BindUniformBufferBase(paramsUBO, 0);
                device.BindStorageImage2D(volume.GetIrradianceAtlas(), 1, RHI::StorageAccess::ReadWrite);
                device.BindStorageImage2D(volume.GetDistanceAtlas(), 2, RHI::StorageAccess::ReadWrite);
                device.BindStorageBuffer(triangleSSBO, 3);

                const RHI::GpuId lightingBuf = LightingUBO::GetInstance().GetGpuBufferId();
                if (lightingBuf != RHI::kInvalidGpuId) {
                    device.BindUniformBufferBase(lightingBuf, 4);
                }

                const unsigned int groups = (static_cast<unsigned int>(params.probesThisFrame) + 63u) / 64u;
                device.DispatchCompute(std::max(groups, 1u), 1, 1);
                device.MemoryBarrierComputeToGraphics();
            }

        }
    }
}
