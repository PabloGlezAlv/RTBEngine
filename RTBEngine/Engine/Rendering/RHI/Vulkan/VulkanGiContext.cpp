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
#include <shaderc/shaderc.h>
#include <array>
#include <fstream>
#include <sstream>
#include <cstring>
#include <functional>
#include <cstdint>

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

            bool VulkanGiContext::Initialize(VkPhysicalDevice physDev, VkDevice dev, std::uint32_t queueFamily, VkQueue queue)
            {
                if (initialized) return true;
                physicalDevice = physDev;
                device = dev;
                graphicsQueueFamily = queueFamily;
                graphicsQueue = queue;
                computeAvailable = true;
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

                for (auto& [id, prog] : computePrograms) {
                    (void)id;
                    if (prog.pipeline) vkDestroyPipeline(device, prog.pipeline, nullptr);
                    if (prog.layout) vkDestroyPipelineLayout(device, prog.layout, nullptr);
                    if (prog.descLayout) vkDestroyDescriptorSetLayout(device, prog.descLayout, nullptr);
                    if (prog.module) vkDestroyShaderModule(device, prog.module, nullptr);
                }
                computePrograms.clear();

                for (auto& [id, buf] : deviceLocalBuffers) {
                    (void)id;
                    if (buf.buffer) vkDestroyBuffer(device, buf.buffer, nullptr);
                    if (buf.memory) vkFreeMemory(device, buf.memory, nullptr);
                }
                deviceLocalBuffers.clear();

                for (auto& [id, buf] : storageBuffers) {
                    (void)id;
                    if (buf.buffer) vkDestroyBuffer(device, buf.buffer, nullptr);
                    if (buf.memory) vkFreeMemory(device, buf.memory, nullptr);
                }
                storageBuffers.clear();

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

            void VulkanGiContext::ExecuteOneShot(std::function<void(VkCommandBuffer)> recordFn) const
            {
                deviceOwner.ExecuteOneShotCommand(recordFn);
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

            void VulkanGiContext::DestroyAccelerationStructures()
            {
                for (VkAccelerationStructureKHR blas : blasList) {
                    if (blas && vkDestroyAccelerationStructureKHR) {
                        vkDestroyAccelerationStructureKHR(device, blas, nullptr);
                    }
                }
                blasList.clear();
                for (VkBuffer buf : blasBuffers) {
                    if (buf) vkDestroyBuffer(device, buf, nullptr);
                }
                blasBuffers.clear();
                for (VkDeviceMemory mem : blasMemories) {
                    if (mem) vkFreeMemory(device, mem, nullptr);
                }
                blasMemories.clear();
                for (DeviceBuffer& buf : rtDeviceBuffers) {
                    if (buf.buffer) vkDestroyBuffer(device, buf.buffer, nullptr);
                    if (buf.memory) vkFreeMemory(device, buf.memory, nullptr);
                }
                rtDeviceBuffers.clear();

                if (tlas && vkDestroyAccelerationStructureKHR) {
                    vkDestroyAccelerationStructureKHR(device, tlas, nullptr);
                    tlas = VK_NULL_HANDLE;
                }
                if (tlasBuffer) { vkDestroyBuffer(device, tlasBuffer, nullptr); tlasBuffer = VK_NULL_HANDLE; }
                if (tlasMemory) { vkFreeMemory(device, tlasMemory, nullptr); tlasMemory = VK_NULL_HANDLE; }
                asBuilt = false;
                cachedAsSignature = 0;
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

            void VulkanGiContext::RebuildAccelerationStructures(GI::RayTracingScene& rtScene, Scene::Scene* scene)
            {
                if (!rayQueryAvailable || !scene) return;
                if (!ddgiTracePipeline) return;


                std::vector<GI::RayTracingMeshInstance> instances;
                for (Scene::MeshRenderer* renderer : scene->GetCachedMeshRenderers()) {
                    if (!renderer || !renderer->IsEnabled()) continue;
                    Scene::GameObject* owner = renderer->GetOwner();
                    if (!owner || !owner->IsActiveInHierarchy()) continue;

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

                std::size_t signature = instances.size();
                for (const GI::RayTracingMeshInstance& inst : instances) {
                    signature ^= reinterpret_cast<std::uintptr_t>(inst.mesh) + 0x9e3779b9 + (signature << 6) + (signature >> 2);
                    for (int i = 0; i < 16; ++i) {
                        std::uint32_t bits = 0;
                        std::memcpy(&bits, &inst.worldMatrix.m[i], sizeof(bits));
                        signature ^= static_cast<std::size_t>(bits) + 0x9e3779b9 + (signature << 6) + (signature >> 2);
                    }
                }

                if (asBuilt && tlas && signature == cachedAsSignature) {
                    return;
                }

                rtScene.Rebuild(scene);
                DestroyAccelerationStructures();
                asBuilt = false;
                cachedAsSignature = 0;

                std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
                std::uint32_t instanceIndex = 0;

                for (const GI::RayTracingMeshInstance& inst : instances) {
                    const auto& verts = inst.mesh->GetCpuVertices();
                    const auto& indices = inst.mesh->GetCpuIndices();
                    if (verts.empty() || indices.empty()) continue;

                    std::vector<RtVertex> rtVerts(verts.size());
                    for (std::size_t i = 0; i < verts.size(); ++i) {
                        rtVerts[i] = { verts[i].position.x, verts[i].position.y, verts[i].position.z };
                    }

                    DeviceBuffer vBuf{}, iBuf{};
                    const VkDeviceSize vSize = rtVerts.size() * sizeof(RtVertex);
                    const VkDeviceSize iSize = indices.size() * sizeof(std::uint32_t);

                    deviceOwner.CreateDeviceLocalBufferRaw(vSize,
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        vBuf.buffer, vBuf.memory);
                    deviceOwner.CreateDeviceLocalBufferRaw(iSize,
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        iBuf.buffer, iBuf.memory);

                    if (!vBuf.buffer || !iBuf.buffer) continue;

                    deviceOwner.UploadToDeviceLocalBuffer(vBuf.buffer, rtVerts.data(), vSize);
                    deviceOwner.UploadToDeviceLocalBuffer(iBuf.buffer, indices.data(), iSize);
                    rtDeviceBuffers.push_back(vBuf);
                    rtDeviceBuffers.push_back(iBuf);

                    VkBufferDeviceAddressInfo addrInfo{};
                    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;

                    addrInfo.buffer = vBuf.buffer;
                    const VkDeviceAddress vAddr = vkGetBufferDeviceAddressKHR(device, &addrInfo);
                    addrInfo.buffer = iBuf.buffer;
                    const VkDeviceAddress iAddr = vkGetBufferDeviceAddressKHR(device, &addrInfo);

                    VkAccelerationStructureGeometryKHR geom{};
                    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
                    geom.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                    geom.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
                    geom.geometry.triangles.vertexData.deviceAddress = vAddr;
                    geom.geometry.triangles.vertexStride = sizeof(RtVertex);
                    geom.geometry.triangles.maxVertex = static_cast<std::uint32_t>(rtVerts.size() - 1);
                    geom.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
                    geom.geometry.triangles.indexData.deviceAddress = iAddr;

                    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
                    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
                    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
                    buildInfo.geometryCount = 1;
                    buildInfo.pGeometries = &geom;

                    const std::uint32_t primitiveCount = static_cast<std::uint32_t>(indices.size() / 3);
                    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
                    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
                    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                        &buildInfo, &primitiveCount, &sizeInfo);

                    VkBuffer blasBuffer = VK_NULL_HANDLE;
                    VkDeviceMemory blasMemory = VK_NULL_HANDLE;
                    deviceOwner.CreateDeviceLocalBufferRaw(sizeInfo.accelerationStructureSize,
                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        blasBuffer, blasMemory);

                    VkBuffer scratchBuffer = VK_NULL_HANDLE;
                    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
                    deviceOwner.CreateDeviceLocalBufferRaw(sizeInfo.buildScratchSize,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        scratchBuffer, scratchMemory);

                    VkAccelerationStructureCreateInfoKHR createInfo{};
                    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
                    createInfo.buffer = blasBuffer;
                    createInfo.size = sizeInfo.accelerationStructureSize;
                    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

                    VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
                    vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &blas);
                    buildInfo.dstAccelerationStructure = blas;

                    addrInfo.buffer = scratchBuffer;
                    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddressKHR(device, &addrInfo);

                    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
                    rangeInfo.primitiveCount = static_cast<std::uint32_t>(indices.size() / 3);
                    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

                    ExecuteOneShot([&](VkCommandBuffer cmd) {
                        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
                    });

                    blasList.push_back(blas);
                    blasBuffers.push_back(blasBuffer);
                    blasMemories.push_back(blasMemory);
                    vkDestroyBuffer(device, scratchBuffer, nullptr);
                    vkFreeMemory(device, scratchMemory, nullptr);

                    VkAccelerationStructureInstanceKHR tlasInst{};
                    std::memset(&tlasInst.transform, 0, sizeof(tlasInst.transform));
                    const float* m = inst.worldMatrix.GetData();
                    // Column-major 3x4 for VkTransformMatrixKHR (row-major layout in spec)
                    for (int col = 0; col < 4; ++col) {
                        for (int row = 0; row < 3; ++row) {
                            tlasInst.transform.matrix[row][col] = m[col * 4 + row];
                        }
                    }
                    tlasInst.instanceCustomIndex = instanceIndex++;
                    tlasInst.mask = 0xFF;
                    tlasInst.instanceShaderBindingTableRecordOffset = 0;
                    tlasInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                    tlasInst.accelerationStructureReference = 0;
                    if (vkGetAccelerationStructureDeviceAddressKHR) {
                        VkAccelerationStructureDeviceAddressInfoKHR addrInfoAS{};
                        addrInfoAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
                        addrInfoAS.accelerationStructure = blas;
                        tlasInst.accelerationStructureReference = vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfoAS);
                    }
                    tlasInstances.push_back(tlasInst);
                }

                if (tlasInstances.empty()) return;

                // Upload instances buffer
                const VkDeviceSize instSize = tlasInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);
                VkBuffer instBuffer = VK_NULL_HANDLE;
                VkDeviceMemory instMemory = VK_NULL_HANDLE;
                deviceOwner.CreateDeviceLocalBufferRaw(instSize,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                        | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                    instBuffer, instMemory);
                deviceOwner.UploadToDeviceLocalBuffer(instBuffer, tlasInstances.data(), instSize);

                VkBufferDeviceAddressInfo addrInfo{};
                addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                addrInfo.buffer = instBuffer;
                const VkDeviceAddress instAddr = vkGetBufferDeviceAddressKHR(device, &addrInfo);

                VkAccelerationStructureGeometryKHR tlasGeom{};
                tlasGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
                tlasGeom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
                tlasGeom.geometry.instances.arrayOfPointers = VK_FALSE;
                tlasGeom.geometry.instances.data.deviceAddress = instAddr;

                VkAccelerationStructureBuildGeometryInfoKHR tlasBuild{};
                tlasBuild.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
                tlasBuild.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
                tlasBuild.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
                tlasBuild.geometryCount = 1;
                tlasBuild.pGeometries = &tlasGeom;

                const std::uint32_t tlasPrimitiveCount = static_cast<std::uint32_t>(tlasInstances.size());
                VkAccelerationStructureBuildSizesInfoKHR tlasSize{};
                tlasSize.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
                vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                    &tlasBuild, &tlasPrimitiveCount, &tlasSize);

                deviceOwner.CreateDeviceLocalBufferRaw(tlasSize.accelerationStructureSize,
                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    tlasBuffer, tlasMemory);

                VkBuffer tlasScratch = VK_NULL_HANDLE;
                VkDeviceMemory tlasScratchMem = VK_NULL_HANDLE;
                deviceOwner.CreateDeviceLocalBufferRaw(tlasSize.buildScratchSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    tlasScratch, tlasScratchMem);

                VkAccelerationStructureCreateInfoKHR tlasCreate{};
                tlasCreate.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
                tlasCreate.buffer = tlasBuffer;
                tlasCreate.size = tlasSize.accelerationStructureSize;
                tlasCreate.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
                vkCreateAccelerationStructureKHR(device, &tlasCreate, nullptr, &tlas);

                tlasBuild.dstAccelerationStructure = tlas;
                addrInfo.buffer = tlasScratch;
                tlasBuild.scratchData.deviceAddress = vkGetBufferDeviceAddressKHR(device, &addrInfo);

                VkAccelerationStructureBuildRangeInfoKHR tlasRange{};
                tlasRange.primitiveCount = static_cast<std::uint32_t>(tlasInstances.size());
                const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRange;

                ExecuteOneShot([&](VkCommandBuffer cmd) {
                    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuild, &pTlasRange);
                });

                vkDestroyBuffer(device, tlasScratch, nullptr);
                vkFreeMemory(device, tlasScratchMem, nullptr);
                vkDestroyBuffer(device, instBuffer, nullptr);
                vkFreeMemory(device, instMemory, nullptr);

                asBuilt = (tlas != VK_NULL_HANDLE);
                cachedAsSignature = signature;

            }

            void VulkanGiContext::UpdateDDGI(GI::DDGIVolume& volume, GI::RayTracingScene& rtScene, Scene::Scene* scene, int frameIndex)
            {
                (void)rtScene;
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

                ExecuteOneShot([&](VkCommandBuffer cmd) {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ddgiTracePipeline);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, giPipelineLayout, 0, 1, &giDescSet, 0, nullptr);
                    const std::uint32_t groups = (params.probesThisFrame + 63) / 64;
                    vkCmdDispatch(cmd, groups, 1, 1);
                });

                // Keep atlases in GENERAL so compute storage + fragment sampling share one layout.
            }

            GpuId VulkanGiContext::CreateStorageImage2D(int width, int height, TextureFormat format)
            {
                return deviceOwner.CreateStorageImage2DInternal(width, height, format);
            }

            void VulkanGiContext::MemoryBarrierComputeToGraphics()
            {
                deviceOwner.MemoryBarrierComputeToGraphicsInternal();
            }

            // Stub implementations for interface completeness
            GpuId VulkanGiContext::CreateComputeProgram(const std::string&) { return kInvalidGpuId; }
            void VulkanGiContext::DestroyComputeProgram(GpuId) {}
            void VulkanGiContext::BindComputeProgram(GpuId) {}
            void VulkanGiContext::DispatchCompute(GpuId, unsigned int, unsigned int, unsigned int) {}
            void VulkanGiContext::BindStorageImage2D(GpuId, unsigned int, StorageAccess) {}
            GpuId VulkanGiContext::CreateStorageBuffer(std::size_t) { return kInvalidGpuId; }
            void VulkanGiContext::UpdateStorageBuffer(GpuId, const void*, std::size_t, std::size_t) {}
            void VulkanGiContext::BindStorageBuffer(GpuId, unsigned int) {}

        }
    }
}
