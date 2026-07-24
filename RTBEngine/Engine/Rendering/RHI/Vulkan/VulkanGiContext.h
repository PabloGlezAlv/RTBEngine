#pragma once

#ifndef VK_NO_PROTOTYPES
#define VK_KHR_acceleration_structure
#define VK_KHR_ray_query
#define VK_KHR_deferred_host_operations
#define VK_KHR_buffer_device_address
#endif

#include <vulkan/vulkan.h>
#include "../RenderTypes.h"
#include "../../GI/GiTypes.h"
#include "../../GI/RayTracingScene.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace RTBEngine {
    namespace Scene {
        class Scene;
    }

    namespace Rendering {
        namespace GI {
            class DDGIVolume;
        }

        namespace RHI {

            class VulkanRenderDevice;

            // Vulkan-specific GI backend: ray query DDGI + compute dispatch.
            class VulkanGiContext {
            public:
                explicit VulkanGiContext(VulkanRenderDevice& owner);
                ~VulkanGiContext();

                bool Initialize(VkPhysicalDevice physDev, VkDevice dev, std::uint32_t queueFamily, VkQueue queue);
                void Shutdown();

                bool IsRayQueryAvailable() const { return rayQueryAvailable; }
                bool IsComputeAvailable() const { return computeAvailable; }
                bool HasTracePipeline() const { return ddgiTracePipeline != VK_NULL_HANDLE; }
                bool EnsureTracePipeline()
                {
                    if (ddgiTracePipeline != VK_NULL_HANDLE) {
                        return true;
                    }
                    if (ddgiCreateAttempted) {
                        return false;
                    }
                    ddgiCreateAttempted = true;
                    return CreateDDGIResources();
                }

                void UpdateDDGI(GI::DDGIVolume& volume, GI::RayTracingScene& rtScene, Scene::Scene* scene, int frameIndex);

                GpuId CreateDeviceLocalBuffer(const void* data, std::size_t size, VkBufferUsageFlags extraUsage);
                std::uint64_t GetBufferDeviceAddress(GpuId buffer) const;

                GpuId CreateComputeProgram(const std::string& computeSource);
                void DestroyComputeProgram(GpuId program);
                void BindComputeProgram(GpuId program);
                void DispatchCompute(GpuId program, unsigned int x, unsigned int y, unsigned int z);

                GpuId CreateStorageImage2D(int width, int height, TextureFormat format);
                void BindStorageImage2D(GpuId texture, unsigned int binding, StorageAccess access);

                GpuId CreateStorageBuffer(std::size_t size);
                void UpdateStorageBuffer(GpuId buffer, const void* data, std::size_t size, std::size_t offset);
                void BindStorageBuffer(GpuId buffer, unsigned int binding);

                void MemoryBarrierComputeToGraphics();

                void RebuildAccelerationStructures(GI::RayTracingScene& rtScene, Scene::Scene* scene);

            private:
                struct ComputeProgramResource {
                    VkShaderModule module = VK_NULL_HANDLE;
                    VkPipeline pipeline = VK_NULL_HANDLE;
                    VkPipelineLayout layout = VK_NULL_HANDLE;
                    VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
                    bool valid = false;
                };

                struct DeviceBuffer {
                    VkBuffer buffer = VK_NULL_HANDLE;
                    VkDeviceMemory memory = VK_NULL_HANDLE;
                    VkDeviceSize size = 0;
                    VkBufferUsageFlags usage = 0;
                };

                bool LoadRayQueryExtensions();
                bool CreateDDGIResources();
                void DestroyDDGIResources();
                void DestroyAccelerationStructures();
                bool LoadTraceShader();
                std::string LoadShaderFile(const char* relativePath) const;
                std::string PreprocessComputeShader(const std::string& source) const;
                VkShaderModule CompileComputeModule(const std::string& source) const;
                void ExecuteOneShot(std::function<void(VkCommandBuffer)> recordFn) const;

                bool BuildBlasForInstance(const GI::RayTracingMeshInstance& inst, VkAccelerationStructureKHR& outBlas,
                                          DeviceBuffer& outVertex, DeviceBuffer& outIndex);
                bool BuildTlas(const std::vector<GI::RayTracingMeshInstance>& instances,
                               const std::vector<VkAccelerationStructureKHR>& blases,
                               VkAccelerationStructureKHR& outTlas);

                VulkanRenderDevice& deviceOwner;
                VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
                VkDevice device = VK_NULL_HANDLE;
                VkQueue graphicsQueue = VK_NULL_HANDLE;
                std::uint32_t graphicsQueueFamily = 0;

                bool rayQueryAvailable = false;
                bool computeAvailable = false;
                bool initialized = false;

                VkDescriptorSetLayout giDescLayout = VK_NULL_HANDLE;
                VkPipelineLayout giPipelineLayout = VK_NULL_HANDLE;
                VkPipeline ddgiTracePipeline = VK_NULL_HANDLE;
                VkDescriptorPool giDescPool = VK_NULL_HANDLE;
                VkDescriptorSet giDescSet = VK_NULL_HANDLE;

                VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
                VkBuffer tlasBuffer = VK_NULL_HANDLE;
                VkDeviceMemory tlasMemory = VK_NULL_HANDLE;

                std::vector<VkAccelerationStructureKHR> blasList;
                std::vector<VkBuffer> blasBuffers;
                std::vector<VkDeviceMemory> blasMemories;
                std::vector<DeviceBuffer> rtDeviceBuffers;

                GpuId traceComputeProgram = kInvalidGpuId;
                GpuId ddgiParamsBuffer = kInvalidGpuId;
                GpuId scratchBuffer = kInvalidGpuId;

                std::unordered_map<GpuId, ComputeProgramResource> computePrograms;
                std::unordered_map<GpuId, DeviceBuffer> deviceLocalBuffers;
                std::unordered_map<GpuId, GpuId> storageImages; // maps to texture id in main device
                std::unordered_map<GpuId, DeviceBuffer> storageBuffers;
                GpuId nextGiId = 100000;
                std::size_t cachedAsSignature = 0;
                bool asBuilt = false;
                bool ddgiCreateAttempted = false;

                // Function pointers for ray tracing
                PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
                PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
                PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
                PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
                PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
                PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
            };

        }
    }
}
