#include "OpenGLDDGIUpdater.h"
#include "../RHI/RenderDevice.h"
#include "../RHI/GraphicsAPI.h"
#include "../../Core/ResourceManager.h"
#include "../../Core/Logger.h"
#include <fstream>
#include <sstream>

namespace RTBEngine {
    namespace Rendering {
        namespace GI {

            namespace {
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
                        std::ostringstream commonSs;
                        commonSs << commonFile.rdbuf();
                        src.replace(pos, includeToken.size(), commonSs.str());
                    }
                    return src;
                }
            }

            bool OpenGLDDGIUpdater::Initialize()
            {
                auto& device = RHI::RenderDevice::Get();
                const auto caps = device.GetGiCapabilities();
                if (!caps.computeShaders || !caps.storageImages) {
                    return false;
                }

                const std::string source = LoadShaderSource("Default/Shaders/ddgi_trace_gl.comp");
                if (source.empty()) {
                    RTB_WARN("OpenGLDDGIUpdater: compute shader missing");
                    return false;
                }

                computeProgram = device.CreateComputeProgram(source);
                paramsUBO = device.CreateBuffer();
                depthCube = device.CreateCubemap();
                ready = computeProgram != RHI::kInvalidGpuId;
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
                if (depthCube != RHI::kInvalidGpuId) {
                    device.DestroyTexture(depthCube);
                    depthCube = RHI::kInvalidGpuId;
                }
                ready = false;
            }

            void OpenGLDDGIUpdater::Update(DDGIVolume& volume, RayTracingScene& rtScene, Scene::Scene* scene, int frameIndex)
            {
                (void)rtScene;
                (void)scene;
                if (!ready) return;

                const DDGISettings& settings = volume.GetSettings();
                if (!settings.enabled) return;

                volume.EnsureGpuResources(RHI::RenderDevice::Get());
                volume.UploadUBO(RHI::RenderDevice::Get());

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
                params.raysPerProbe = kDDGIRaysPerProbe;
                params.probesThisFrame = std::min(kDDGIMaxProbesPerFrame, params.probeCount);
                params.probeRadius = settings.probeRadius;

                auto& device = RHI::RenderDevice::Get();
                device.SetUniformBufferData(paramsUBO, &params, sizeof(params), RHI::BufferUsage::Dynamic);
                device.BindComputeProgram(computeProgram);
                device.BindUniformBufferBase(paramsUBO, 0);
                device.BindStorageImage2D(volume.GetIrradianceAtlas(), 1, RHI::StorageAccess::ReadWrite);
                device.BindStorageImage2D(volume.GetDistanceAtlas(), 2, RHI::StorageAccess::ReadWrite);
                device.BindCubemap(depthCube, 3);
                const unsigned int groups = (params.probesThisFrame + 63u) / 64u;
                device.DispatchCompute(groups, 1, 1);
                device.MemoryBarrierComputeToGraphics();
            }

        }
    }
}
