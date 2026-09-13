#include "VulkanGiContext.h"
#include "VulkanRenderDevice.h"
#include "../../../Core/ResourceManager.h"
#include "../../../Core/Logger.h"
#include "../../Vertex.h"
#include "../../Mesh.h"
#include "../../GI/DDGIVolume.h"
#include "../../Lighting/LightingUBO.h"
#include "../../../Scene/Scene.h"
#include "../../../Scene/MeshRenderer.h"
#include "../../../Scene/StaticFlagsUtil.h"
#include "../../../Scene/GameObject.h"
#include <shaderc/shaderc.h>
#include <array>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdint>
#include <unordered_set>

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            namespace {

                constexpr std::array<const char*, 4> kRtExtensions = {
                    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                    VK_KHR_RAY_QUERY_EXTENSION_NAME,
                    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
                };

                struct RtVertex {
                    float px, py, pz;
                };

                std::size_t ComputeMeshGeometrySignature(Mesh* mesh)
                {
                    if (!mesh) {
                        return 0;
                    }
                    const auto& verts = mesh->GetCpuVertices();
                    const auto& indices = mesh->GetCpuIndices();
                    std::size_t signature = verts.size();
                    signature ^= indices.size() + 0x9e3779b9 + (signature << 6) + (signature >> 2);
                    for (std::size_t i = 0; i < verts.size(); ++i) {
                        std::uint32_t bits = 0;
                        std::memcpy(&bits, &verts[i].position.x, sizeof(bits));
                        signature ^= static_cast<std::size_t>(bits) + 0x9e3779b9 + (signature << 6) + (signature >> 2);
                        if (i >= 31) {
                            break;
                        }
                    }
                    return signature;
                }

                std::size_t ComputeInstanceSignature(const std::vector<GI::RayTracingMeshInstance>& instances)
                {
                    std::size_t signature = instances.size();
                    for (const GI::RayTracingMeshInstance& inst : instances) {
                        signature ^= reinterpret_cast<std::uintptr_t>(inst.mesh) + 0x9e3779b9 + (signature << 6) + (signature >> 2);
                        for (int i = 0; i < 16; ++i) {
                            std::uint32_t bits = 0;
                            std::memcpy(&bits, &inst.worldMatrix.m[i], sizeof(bits));
                            signature ^= static_cast<std::size_t>(bits) + 0x9e3779b9 + (signature << 6) + (signature >> 2);
                        }
                    }
                    return signature;
                }

            } // namespace

            VulkanGiContext::VulkanGiContext(VulkanRenderDevice& owner) : deviceOwner(owner) {}

            VulkanGiContext::~VulkanGiContext()
            {
                Shutdown();
            }

            bool VulkanGiContext::LoadRayQueryExtensions()
            {
                vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
                    vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
                vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
                    vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
                vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
                    vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
                vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
                    vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
                vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
                    vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
                vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(
                    vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));

                rayQueryAvailable = vkCreateAccelerationStructureKHR && vkCmdBuildAccelerationStructuresKHR
                    && vkGetBufferDeviceAddressKHR;
                return rayQueryAvailable;
            }

            bool VulkanGiContext::Initialize(VkDevice dev)
            {
                if (initialized) return true;
                device = dev;
                LoadRayQueryExtensions();
                // DDGI pipeline is created lazily on first update (ResourceManager/cwd ready).
                initialized = true;
                return true;
            }

            void VulkanGiContext::Shutdown()
            {
                if (device != VK_NULL_HANDLE) {
                    vkDeviceWaitIdle(device);
                }
                DestroyDDGIResources();

                for (auto& [id, buf] : deviceLocalBuffers) {
                    (void)id;
                    if (buf.buffer) vkDestroyBuffer(device, buf.buffer, nullptr);
                    if (buf.memory) vkFreeMemory(device, buf.memory, nullptr);
                }
                deviceLocalBuffers.clear();

                initialized = false;
            }

            std::string VulkanGiContext::LoadShaderFile(const char* relativePath) const
            {
                const std::string resolved = Core::ResourceManager::GetInstance().ResolvePathForRead(relativePath);
                const std::filesystem::path candidates[] = {
                    std::filesystem::path(resolved),
                    std::filesystem::current_path() / relativePath,
                    std::filesystem::current_path() / "RTBEngine_SDK" / relativePath,
                };

                for (const std::filesystem::path& candidate : candidates) {
                    std::ifstream file(candidate, std::ios::binary);
                    if (!file) {
                        continue;
                    }
                    std::ostringstream ss;
                    ss << file.rdbuf();
                    return ss.str();
                }

                RTB_WARN(std::string("VulkanGiContext: shader not found: ") + relativePath);
                return {};
            }

            std::string VulkanGiContext::PreprocessComputeShader(const std::string& source) const
            {
                std::string result = source;
                const std::string includePath = Core::ResourceManager::GetInstance().ResolvePathForRead("Default/Shaders/ddgi_common.glsl");
                std::ifstream inc(includePath);
                if (!inc) {
                    // Fallback next to cwd / SDK
                    inc.open(std::filesystem::current_path() / "Default/Shaders/ddgi_common.glsl");
                    if (!inc) {
                        inc.open(std::filesystem::current_path() / "RTBEngine_SDK/Default/Shaders/ddgi_common.glsl");
                    }
                }
                if (!inc) return result;
                std::ostringstream commonSs;
                commonSs << inc.rdbuf();
                std::string common = commonSs.str();
                // Includes must not inject another #version.
                const std::string versionToken = "#version";
                std::size_t versionPos = common.find(versionToken);
                if (versionPos != std::string::npos) {
                    const std::size_t lineEnd = common.find('\n', versionPos);
                    common.erase(versionPos, (lineEnd == std::string::npos) ? common.size() - versionPos : lineEnd - versionPos + 1);
                }
                const std::string token = "#include \"ddgi_common.glsl\"";
                const auto pos = result.find(token);
                if (pos != std::string::npos) {
                    result.replace(pos, token.size(), common);
                }
                return result;
            }

            VkShaderModule VulkanGiContext::CompileComputeModule(const std::string& source) const
            {
                shaderc_compiler_t compiler = shaderc_compiler_initialize();
                if (!compiler) return VK_NULL_HANDLE;

                shaderc_compile_options_t options = shaderc_compile_options_initialize();
                shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
                shaderc_compile_options_set_target_spirv(options, shaderc_spirv_version_1_4);

                shaderc_compilation_result_t result = shaderc_compile_into_spv(
                    compiler, source.c_str(), source.size(),
                    shaderc_compute_shader, "ddgi.comp", "main", options);

                shaderc_compile_options_release(options);

                if (!result || shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
                    if (result) {
                        const char* err = shaderc_result_get_error_message(result);
                        RTB_ERROR(std::string("DDGI compute compile failed: ") + (err ? err : "unknown"));
                        shaderc_result_release(result);
                    }
                    shaderc_compiler_release(compiler);
                    return VK_NULL_HANDLE;
                }

                VkShaderModuleCreateInfo ci{};
                ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                ci.codeSize = shaderc_result_get_length(result);
                ci.pCode = reinterpret_cast<const std::uint32_t*>(shaderc_result_get_bytes(result));
                VkShaderModule module = VK_NULL_HANDLE;
                vkCreateShaderModule(device, &ci, nullptr, &module);
                shaderc_result_release(result);
                shaderc_compiler_release(compiler);
                return module;
            }

            bool VulkanGiContext::CreateDDGIResources()
            {

                if (!rayQueryAvailable) {
                    return false;
                }

                std::array<VkDescriptorSetLayoutBinding, 5> bindings{};
                bindings[0] = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
                bindings[1] = { 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
                bindings[2] = { 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
                bindings[3] = { 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
                bindings[4] = { 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

                VkDescriptorSetLayoutCreateInfo layoutInfo{};
                layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
                layoutInfo.pBindings = bindings.data();
                if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &giDescLayout) != VK_SUCCESS) {
                    return false;
                }

                VkPipelineLayoutCreateInfo plInfo{};
                plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
                plInfo.setLayoutCount = 1;
                plInfo.pSetLayouts = &giDescLayout;
                if (vkCreatePipelineLayout(device, &plInfo, nullptr, &giPipelineLayout) != VK_SUCCESS) {
                    return false;
                }

                std::string source = LoadShaderFile("Default/Shaders/ddgi_trace.comp");
                if (source.empty()) {
                    return false;
                }
                source = PreprocessComputeShader(source);
                VkShaderModule module = CompileComputeModule(source);
                if (!module) {
                    DestroyDDGIResources();
                    return false;
                }

                VkPipelineShaderStageCreateInfo stage{};
                stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                stage.module = module;
                stage.pName = "main";

                VkComputePipelineCreateInfo pipeInfo{};
                pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                pipeInfo.stage = stage;
                pipeInfo.layout = giPipelineLayout;
                if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &ddgiTracePipeline) != VK_SUCCESS) {
                    vkDestroyShaderModule(device, module, nullptr);
                    DestroyDDGIResources();
                    return false;
                }
                vkDestroyShaderModule(device, module, nullptr);

                VkDescriptorPoolSize poolSizes[] = {
                    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4 },
                    { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 }
                };
                VkDescriptorPoolCreateInfo poolInfo{};
                poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                poolInfo.poolSizeCount = 3;
                poolInfo.pPoolSizes = poolSizes;
                poolInfo.maxSets = 1;
                vkCreateDescriptorPool(device, &poolInfo, nullptr, &giDescPool);

                VkDescriptorSetAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = giDescPool;
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts = &giDescLayout;
                vkAllocateDescriptorSets(device, &allocInfo, &giDescSet);


                return true;
            }

            void VulkanGiContext::OrphanBlasEntry(CachedBlas& entry)
            {
                if (entry.blas) {
                    deviceOwner.OrphanAccelerationStructure(entry.blas);
                }
                deviceOwner.OrphanGpuBuffer(entry.blasBuffer, entry.blasMemory);
                deviceOwner.OrphanGpuBuffer(entry.vertices.buffer, entry.vertices.memory);
                deviceOwner.OrphanGpuBuffer(entry.indices.buffer, entry.indices.memory);
                entry = {};
            }

            void VulkanGiContext::OrphanTlasResources()
            {
                if (tlas) {
                    deviceOwner.OrphanAccelerationStructure(tlas);
                }
                deviceOwner.OrphanGpuBuffer(tlasBuffer, tlasMemory);
                deviceOwner.OrphanGpuBuffer(tlasInstanceBuffer, tlasInstanceMemory);
                tlas = VK_NULL_HANDLE;
                tlasBuffer = VK_NULL_HANDLE;
                tlasMemory = VK_NULL_HANDLE;
                tlasInstanceBuffer = VK_NULL_HANDLE;
                tlasInstanceMemory = VK_NULL_HANDLE;
            }

            void VulkanGiContext::DestroyAccelerationStructures()
            {
                for (auto& [mesh, entry] : blasCache) {
                    (void)mesh;
                    if (entry.blas && vkDestroyAccelerationStructureKHR) {
                        vkDestroyAccelerationStructureKHR(device, entry.blas, nullptr);
                    }
                    if (entry.blasBuffer) vkDestroyBuffer(device, entry.blasBuffer, nullptr);
                    if (entry.blasMemory) vkFreeMemory(device, entry.blasMemory, nullptr);
                    if (entry.vertices.buffer) vkDestroyBuffer(device, entry.vertices.buffer, nullptr);
                    if (entry.vertices.memory) vkFreeMemory(device, entry.vertices.memory, nullptr);
                    if (entry.indices.buffer) vkDestroyBuffer(device, entry.indices.buffer, nullptr);
                    if (entry.indices.memory) vkFreeMemory(device, entry.indices.memory, nullptr);
                }
                blasCache.clear();

                if (tlas && vkDestroyAccelerationStructureKHR) {
                    vkDestroyAccelerationStructureKHR(device, tlas, nullptr);
                    tlas = VK_NULL_HANDLE;
                }
                if (tlasBuffer) { vkDestroyBuffer(device, tlasBuffer, nullptr); tlasBuffer = VK_NULL_HANDLE; }
                if (tlasMemory) { vkFreeMemory(device, tlasMemory, nullptr); tlasMemory = VK_NULL_HANDLE; }
                if (tlasInstanceBuffer) { vkDestroyBuffer(device, tlasInstanceBuffer, nullptr); tlasInstanceBuffer = VK_NULL_HANDLE; }
                if (tlasInstanceMemory) { vkFreeMemory(device, tlasInstanceMemory, nullptr); tlasInstanceMemory = VK_NULL_HANDLE; }
                asBuilt = false;
                cachedInstanceSignature = 0;
            }

            void VulkanGiContext::DestroyDDGIResources()
            {
                DestroyAccelerationStructures();
                if (ddgiTracePipeline) { vkDestroyPipeline(device, ddgiTracePipeline, nullptr); ddgiTracePipeline = VK_NULL_HANDLE; }
                if (giPipelineLayout) { vkDestroyPipelineLayout(device, giPipelineLayout, nullptr); giPipelineLayout = VK_NULL_HANDLE; }
                if (giDescLayout) { vkDestroyDescriptorSetLayout(device, giDescLayout, nullptr); giDescLayout = VK_NULL_HANDLE; }
                if (giDescPool) { vkDestroyDescriptorPool(device, giDescPool, nullptr); giDescPool = VK_NULL_HANDLE; }
                giDescSet = VK_NULL_HANDLE;
            }

            GpuId VulkanGiContext::CreateDeviceLocalBuffer(const void* data, std::size_t size, VkBufferUsageFlags extraUsage)
            {
                const GpuId id = nextGiId++;
                DeviceBuffer res{};
                const VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | extraUsage;

                deviceOwner.CreateDeviceLocalBufferRaw(size, usage, res.buffer, res.memory);
                if (!res.buffer) return kInvalidGpuId;

                if (data && size > 0) {
                    deviceOwner.UploadToDeviceLocalBuffer(res.buffer, data, size);
                }
                res.size = size;
                res.usage = usage;
                deviceLocalBuffers[id] = res;
                return id;
            }

            std::uint64_t VulkanGiContext::GetBufferDeviceAddress(GpuId buffer) const
            {
                auto it = deviceLocalBuffers.find(buffer);
                if (it == deviceLocalBuffers.end() || !vkGetBufferDeviceAddressKHR) return 0;
                VkBufferDeviceAddressInfo info{};
                info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                info.buffer = it->second.buffer;
                return vkGetBufferDeviceAddressKHR(device, &info);
            }

            bool VulkanGiContext::PrepareBlasBuild(Mesh* mesh, std::size_t geometrySignature, CachedBlas& entry, FrameBlasBuild& outBuild)
            {
                const auto& verts = mesh->GetCpuVertices();
                const auto& indices = mesh->GetCpuIndices();
                if (verts.empty() || indices.empty()) {
                    return false;
                }

                std::vector<RtVertex> rtVerts(verts.size());
                for (std::size_t i = 0; i < verts.size(); ++i) {
                    rtVerts[i] = { verts[i].position.x, verts[i].position.y, verts[i].position.z };
                }

                const VkDeviceSize vertexBytes = rtVerts.size() * sizeof(RtVertex);
                const VkDeviceSize indexBytes = indices.size() * sizeof(std::uint32_t);
                const VkBufferUsageFlags geoUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                    | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                    | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

                deviceOwner.CreateDeviceLocalBufferRaw(vertexBytes, geoUsage, entry.vertices.buffer, entry.vertices.memory);
                deviceOwner.CreateDeviceLocalBufferRaw(indexBytes, geoUsage, entry.indices.buffer, entry.indices.memory);
                if (!entry.vertices.buffer || !entry.indices.buffer) {
                    return false;
                }
                entry.vertices.size = vertexBytes;
                entry.indices.size = indexBytes;

                if (!deviceOwner.CreateHostStagingBuffer(rtVerts.data(), vertexBytes, outBuild.vertexStaging, outBuild.vertexStagingMemory)) {
                    return false;
                }
                if (!deviceOwner.CreateHostStagingBuffer(indices.data(), indexBytes, outBuild.indexStaging, outBuild.indexStagingMemory)) {
                    return false;
                }
                outBuild.vertexBytes = vertexBytes;
                outBuild.indexBytes = indexBytes;

                VkBufferDeviceAddressInfo addrInfo{};
                addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                addrInfo.buffer = entry.vertices.buffer;
                const VkDeviceAddress vertexAddress = vkGetBufferDeviceAddressKHR(device, &addrInfo);
                addrInfo.buffer = entry.indices.buffer;
                const VkDeviceAddress indexAddress = vkGetBufferDeviceAddressKHR(device, &addrInfo);

                outBuild.geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                outBuild.geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                outBuild.geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
                outBuild.geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                outBuild.geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
                outBuild.geometry.geometry.triangles.vertexData.deviceAddress = vertexAddress;
                outBuild.geometry.geometry.triangles.vertexStride = sizeof(RtVertex);
                outBuild.geometry.geometry.triangles.maxVertex = static_cast<std::uint32_t>(rtVerts.size() - 1);
                outBuild.geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
                outBuild.geometry.geometry.triangles.indexData.deviceAddress = indexAddress;

                outBuild.buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
                outBuild.buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                outBuild.buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
                outBuild.buildInfo.geometryCount = 1;
                outBuild.buildInfo.pGeometries = &outBuild.geometry;

                const std::uint32_t primitiveCount = static_cast<std::uint32_t>(indices.size() / 3);
                VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
                sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
                vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                    &outBuild.buildInfo, &primitiveCount, &sizeInfo);

                deviceOwner.CreateDeviceLocalBufferRaw(sizeInfo.accelerationStructureSize,
                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    entry.blasBuffer, entry.blasMemory);
                deviceOwner.CreateDeviceLocalBufferRaw(sizeInfo.buildScratchSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    outBuild.scratchBuffer, outBuild.scratchMemory);
                if (!entry.blasBuffer || !outBuild.scratchBuffer) {
                    return false;
                }

                VkAccelerationStructureCreateInfoKHR createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
                createInfo.buffer = entry.blasBuffer;
                createInfo.size = sizeInfo.accelerationStructureSize;
                createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                if (vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &entry.blas) != VK_SUCCESS) {
                    return false;
                }

                outBuild.buildInfo.dstAccelerationStructure = entry.blas;
                addrInfo.buffer = outBuild.scratchBuffer;
                outBuild.buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddressKHR(device, &addrInfo);
                outBuild.rangeInfo.primitiveCount = primitiveCount;
                outBuild.mesh = mesh;

                entry.geometrySignature = geometrySignature;
                entry.built = false;
                if (vkGetAccelerationStructureDeviceAddressKHR) {
                    VkAccelerationStructureDeviceAddressInfoKHR asAddrInfo{};
                    asAddrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
                    asAddrInfo.accelerationStructure = entry.blas;
                    entry.blasDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &asAddrInfo);
                }
                return true;
            }

            bool VulkanGiContext::PrepareTlasBuild(const std::vector<GI::RayTracingMeshInstance>& instances, FrameTlasBuild& outBuild)
            {
                std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
                std::uint32_t instanceIndex = 0;
                for (const GI::RayTracingMeshInstance& inst : instances) {
                    if (!inst.mesh) {
                        continue;
                    }
                    auto cacheIt = blasCache.find(inst.mesh);
                    if (cacheIt == blasCache.end() || !cacheIt->second.blas) {
                        continue;
                    }
                    const CachedBlas& cached = cacheIt->second;

                    VkAccelerationStructureInstanceKHR tlasInst{};
                    std::memset(&tlasInst.transform, 0, sizeof(tlasInst.transform));
                    const float* m = inst.worldMatrix.GetData();
                    for (int col = 0; col < 4; ++col) {
                        for (int row = 0; row < 3; ++row) {
                            tlasInst.transform.matrix[row][col] = m[col * 4 + row];
                        }
                    }
                    tlasInst.instanceCustomIndex = instanceIndex++;
                    tlasInst.mask = 0xFF;
                    tlasInst.instanceShaderBindingTableRecordOffset = 0;
                    tlasInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                    tlasInst.accelerationStructureReference = cached.blasDeviceAddress;
                    tlasInstances.push_back(tlasInst);
                }

                if (tlasInstances.empty()) {
                    return false;
                }

                const VkDeviceSize instanceBytes = tlasInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);
                deviceOwner.CreateDeviceLocalBufferRaw(instanceBytes,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                        | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                    tlasInstanceBuffer, tlasInstanceMemory);
                if (!tlasInstanceBuffer) {
                    return false;
                }

                if (!deviceOwner.CreateHostStagingBuffer(tlasInstances.data(), instanceBytes,
                        outBuild.instanceStaging, outBuild.instanceStagingMemory)) {
                    return false;
                }
                outBuild.instanceBytes = instanceBytes;

                VkBufferDeviceAddressInfo addrInfo{};
                addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                addrInfo.buffer = tlasInstanceBuffer;
                const VkDeviceAddress instanceAddress = vkGetBufferDeviceAddressKHR(device, &addrInfo);

                outBuild.geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                outBuild.geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
                outBuild.geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
                outBuild.geometry.geometry.instances.arrayOfPointers = VK_FALSE;
                outBuild.geometry.geometry.instances.data.deviceAddress = instanceAddress;

                outBuild.buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
                outBuild.buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
                outBuild.buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
                outBuild.buildInfo.geometryCount = 1;
                outBuild.buildInfo.pGeometries = &outBuild.geometry;

                const std::uint32_t tlasPrimitiveCount = static_cast<std::uint32_t>(tlasInstances.size());
                VkAccelerationStructureBuildSizesInfoKHR tlasSize{};
                tlasSize.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
                vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                    &outBuild.buildInfo, &tlasPrimitiveCount, &tlasSize);

                deviceOwner.CreateDeviceLocalBufferRaw(tlasSize.accelerationStructureSize,
                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    tlasBuffer, tlasMemory);
                deviceOwner.CreateDeviceLocalBufferRaw(tlasSize.buildScratchSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    outBuild.scratchBuffer, outBuild.scratchMemory);
                if (!tlasBuffer || !outBuild.scratchBuffer) {
                    return false;
                }

                VkAccelerationStructureCreateInfoKHR tlasCreate{};
                tlasCreate.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
                tlasCreate.buffer = tlasBuffer;
                tlasCreate.size = tlasSize.accelerationStructureSize;
                tlasCreate.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
                if (vkCreateAccelerationStructureKHR(device, &tlasCreate, nullptr, &tlas) != VK_SUCCESS) {
                    return false;
                }

                outBuild.buildInfo.dstAccelerationStructure = tlas;
                addrInfo.buffer = outBuild.scratchBuffer;
                outBuild.buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddressKHR(device, &addrInfo);
                outBuild.rangeInfo.primitiveCount = tlasPrimitiveCount;
                outBuild.needed = true;
                return true;
            }

            void VulkanGiContext::RebuildAccelerationStructures(GI::RayTracingScene& rtScene, Scene::Scene* scene)
            {
                if (!rayQueryAvailable || !scene) return;
                if (!ddgiTracePipeline) return;

                std::vector<GI::RayTracingMeshInstance> instances;
                for (Scene::MeshRenderer* renderer : scene->GetCachedMeshRenderers()) {
                    if (!Scene::RendererContributesGI(renderer)) continue;

                    Scene::GameObject* owner = renderer->GetOwner();
                    if (!owner) continue;

                    const auto addMeshInstance = [&](Mesh* mesh) {
                        if (!mesh || mesh->GetCpuIndices().empty()) return;
                        GI::RayTracingMeshInstance inst{};
                        inst.mesh = mesh;
                        inst.worldMatrix = owner->GetWorldMatrix();
                        instances.push_back(inst);
                    };

                    if (renderer->IsMultiMesh()) {
                        for (Mesh* mesh : renderer->GetMeshes()) {
                            addMeshInstance(mesh);
                        }
                    } else {
                        addMeshInstance(renderer->GetMesh());
                    }
                }

                const std::size_t instanceSignature = ComputeInstanceSignature(instances);

                std::unordered_set<Mesh*> sceneMeshes;
                for (const GI::RayTracingMeshInstance& inst : instances) {
                    if (inst.mesh) {
                        sceneMeshes.insert(inst.mesh);
                    }
                }

                for (auto it = blasCache.begin(); it != blasCache.end();) {
                    if (sceneMeshes.count(it->first) == 0) {
                        OrphanBlasEntry(it->second);
                        it = blasCache.erase(it);
                    } else {
                        ++it;
                    }
                }

                std::vector<FrameBlasBuild> blasBuilds;
                bool blasRebuilt = false;
                for (Mesh* mesh : sceneMeshes) {
                    const std::size_t geometrySignature = ComputeMeshGeometrySignature(mesh);
                    CachedBlas& cached = blasCache[mesh];
                    if (cached.built && cached.geometrySignature == geometrySignature && cached.blas) {
                        continue;
                    }
                    if (cached.blas) {
                        OrphanBlasEntry(cached);
                        cached = {};
                    }

                    FrameBlasBuild buildJob{};
                    if (!PrepareBlasBuild(mesh, geometrySignature, cached, buildJob)) {
                        blasCache.erase(mesh);
                        continue;
                    }
                    blasBuilds.push_back(std::move(buildJob));
                    blasRebuilt = true;
                }

                const bool needTlas = blasRebuilt || !asBuilt || !tlas || instanceSignature != cachedInstanceSignature;
                if (!blasRebuilt && !needTlas) {
                    return;
                }

                if (instances.empty()) {
                    OrphanTlasResources();
                    asBuilt = false;
                    cachedInstanceSignature = 0;
                    return;
                }

                rtScene.Rebuild(scene);

                FrameTlasBuild tlasBuild{};
                if (needTlas) {
                    OrphanTlasResources();
                    if (!PrepareTlasBuild(instances, tlasBuild)) {
                        asBuilt = false;
                        cachedInstanceSignature = 0;
                        return;
                    }
                }

                deviceOwner.RecordToFrameCommandBuffer([&, blasBuilds = std::move(blasBuilds), tlasBuild = std::move(tlasBuild)] (VkCommandBuffer cmd) {
                    bool didTransfer = false;

                    for (const FrameBlasBuild& job : blasBuilds) {
                        auto cacheIt = blasCache.find(job.mesh);
                        if (cacheIt == blasCache.end()) {
                            continue;
                        }
                        CachedBlas& cached = cacheIt->second;
                        if (job.vertexStaging) {
                            deviceOwner.RecordBufferCopyAndOrphanStaging(cmd, job.vertexStaging, job.vertexStagingMemory,
                                cached.vertices.buffer, job.vertexBytes);
                            didTransfer = true;
                        }
                        if (job.indexStaging) {
                            deviceOwner.RecordBufferCopyAndOrphanStaging(cmd, job.indexStaging, job.indexStagingMemory,
                                cached.indices.buffer, job.indexBytes);
                            didTransfer = true;
                        }
                    }

                    if (tlasBuild.needed && tlasBuild.instanceStaging) {
                        deviceOwner.RecordBufferCopyAndOrphanStaging(cmd, tlasBuild.instanceStaging, tlasBuild.instanceStagingMemory,
                            tlasInstanceBuffer, tlasBuild.instanceBytes);
                        didTransfer = true;
                    }

                    if (didTransfer) {
                        VkMemoryBarrier transferBarrier{};
                        transferBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                        transferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                        transferBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                        vkCmdPipelineBarrier(cmd,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                            0, 1, &transferBarrier, 0, nullptr, 0, nullptr);
                    }

                    bool didAsBuild = false;
                    for (const FrameBlasBuild& job : blasBuilds) {
                        auto cacheIt = blasCache.find(job.mesh);
                        if (cacheIt == blasCache.end()) {
                            continue;
                        }
                        FrameBlasBuild mutableJob = job;
                        const VkAccelerationStructureBuildRangeInfoKHR* range = &mutableJob.rangeInfo;
                        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &mutableJob.buildInfo, &range);
                        deviceOwner.OrphanGpuBuffer(mutableJob.scratchBuffer, mutableJob.scratchMemory);
                        cacheIt->second.built = true;
                        didAsBuild = true;
                    }

                    if (tlasBuild.needed) {
                        FrameTlasBuild mutableTlas = tlasBuild;
                        const VkAccelerationStructureBuildRangeInfoKHR* range = &mutableTlas.rangeInfo;
                        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &mutableTlas.buildInfo, &range);
                        deviceOwner.OrphanGpuBuffer(mutableTlas.scratchBuffer, mutableTlas.scratchMemory);
                        didAsBuild = true;
                    }

                    if (didAsBuild) {
                        VkMemoryBarrier asBarrier{};
                        asBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                        asBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                        asBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
                        vkCmdPipelineBarrier(cmd,
                            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                            0, 1, &asBarrier, 0, nullptr, 0, nullptr);
                    }
                });

                cachedInstanceSignature = instanceSignature;
                asBuilt = (tlas != VK_NULL_HANDLE);
            }

            void VulkanGiContext::UpdateDDGI(GI::DDGIVolume& volume, int frameIndex)
            {
                if (!rayQueryAvailable || !ddgiTracePipeline || !tlas) return;

                const GI::DDGISettings& settings = volume.GetSettings();
                if (!settings.enabled) return;

                volume.EnsureGpuResources(deviceOwner);
                volume.UploadUBO(deviceOwner);

                struct DDGIParamsGPU {
                    alignas(16) float origin[3];
                    float hysteresis;
                    alignas(16) float spacing[3];
                    float normalBias;
                    alignas(16) int gridDims[3];
                    int probeCount;
                    int frameIndex;
                    int raysPerProbe;
                    int probesThisFrame;
                    float probeRadius;
                    float _pad;
                } params{};

                params.origin[0] = settings.origin.x;
                params.origin[1] = settings.origin.y;
                params.origin[2] = settings.origin.z;
                params.hysteresis = settings.hysteresis;
                params.spacing[0] = settings.extent.x / std::max(settings.gridX, 1);
                params.spacing[1] = settings.extent.y / std::max(settings.gridY, 1);
                params.spacing[2] = settings.extent.z / std::max(settings.gridZ, 1);
                params.gridDims[0] = settings.gridX;
                params.gridDims[1] = settings.gridY;
                params.gridDims[2] = settings.gridZ;
                params.probeCount = GI::DDGIProbeCount(settings);
                params.frameIndex = frameIndex;
                params.raysPerProbe = GI::kDDGIRaysPerProbe;
                params.probesThisFrame = std::min(GI::kDDGIMaxProbesPerFrame, params.probeCount);
                params.probeRadius = settings.probeRadius;
                params.normalBias = settings.normalBias;


                if (ddgiParamsBuffer == kInvalidGpuId) {
                    ddgiParamsBuffer = deviceOwner.CreateBuffer();
                }
                deviceOwner.SetUniformBufferData(ddgiParamsBuffer, &params, sizeof(params), RHI::BufferUsage::Dynamic);

                VkBuffer paramsBuf = deviceOwner.GetBufferHandle(ddgiParamsBuffer);
                VkImageView irrView = deviceOwner.GetTextureImageView(volume.GetIrradianceAtlas());
                VkImageView distView = deviceOwner.GetTextureImageView(volume.GetDistanceAtlas());

                VkDescriptorBufferInfo paramsInfo{ paramsBuf, 0, sizeof(DDGIParamsGPU) };
                VkWriteDescriptorSet writes[5]{};
                writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[0].dstSet = giDescSet;
                writes[0].dstBinding = 0;
                writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                writes[0].descriptorCount = 1;
                writes[0].pBufferInfo = &paramsInfo;

                VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
                asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                asWrite.accelerationStructureCount = 1;
                asWrite.pAccelerationStructures = &tlas;
                writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[1].dstSet = giDescSet;
                writes[1].dstBinding = 1;
                writes[1].descriptorCount = 1;
                writes[1].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                writes[1].pNext = &asWrite;

                VkDescriptorImageInfo irrInfo{ VK_NULL_HANDLE, irrView, VK_IMAGE_LAYOUT_GENERAL };
                writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[2].dstSet = giDescSet;
                writes[2].dstBinding = 2;
                writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                writes[2].descriptorCount = 1;
                writes[2].pImageInfo = &irrInfo;

                VkDescriptorImageInfo distInfo{ VK_NULL_HANDLE, distView, VK_IMAGE_LAYOUT_GENERAL };
                writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[3].dstSet = giDescSet;
                writes[3].dstBinding = 3;
                writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                writes[3].descriptorCount = 1;
                writes[3].pImageInfo = &distInfo;

                // Lighting UBO for direct eval in trace shader
                VkBuffer lightBuf = deviceOwner.GetBufferHandle(LightingUBO::GetInstance().GetGpuBufferId());
                VkDescriptorBufferInfo lightInfo{ lightBuf, 0, VK_WHOLE_SIZE };
                writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[4].dstSet = giDescSet;
                writes[4].dstBinding = 4;
                writes[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                writes[4].descriptorCount = 1;
                writes[4].pBufferInfo = &lightInfo;

                vkUpdateDescriptorSets(device, 5, writes, 0, nullptr);

                deviceOwner.UpdateStorageImageLayout(volume.GetIrradianceAtlas(), VK_IMAGE_LAYOUT_GENERAL);
                deviceOwner.UpdateStorageImageLayout(volume.GetDistanceAtlas(), VK_IMAGE_LAYOUT_GENERAL);

                deviceOwner.RecordToFrameCommandBuffer([&](VkCommandBuffer cmd) {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ddgiTracePipeline);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, giPipelineLayout, 0, 1, &giDescSet, 0, nullptr);
                    const std::uint32_t groups = (params.probesThisFrame + 63) / 64;
                    vkCmdDispatch(cmd, groups, 1, 1);
                });
            }

            GpuId VulkanGiContext::CreateStorageImage2D(int width, int height, TextureFormat format)
            {
                return deviceOwner.CreateStorageImage2DInternal(width, height, format);
            }

            void VulkanGiContext::MemoryBarrierComputeToGraphics()
            {
                deviceOwner.MemoryBarrierComputeToGraphicsInternal();
            }

            GpuId VulkanGiContext::CreateComputeProgram(const std::string&) { return kInvalidGpuId; }
            void VulkanGiContext::DestroyComputeProgram(GpuId) {}
            VkPipeline VulkanGiContext::GetComputePipeline(GpuId) const
            {
                return VK_NULL_HANDLE;
            }
            void VulkanGiContext::BindStorageImage2D(GpuId, unsigned int, StorageAccess) {}
            GpuId VulkanGiContext::CreateStorageBuffer(std::size_t) { return kInvalidGpuId; }
            void VulkanGiContext::UpdateStorageBuffer(GpuId, const void*, std::size_t, std::size_t) {}
            void VulkanGiContext::BindStorageBuffer(GpuId, unsigned int) {}

        }
    }
}
