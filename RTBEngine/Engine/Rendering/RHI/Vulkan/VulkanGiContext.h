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
#include <string>
#include <unordered_map>
#include <vector>

namespace RTBEngine {
    namespace Scene {
        class Scene;
    }

    namespace Rendering {
        class Mesh;

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

                bool Initialize(VkDevice dev);
                void Shutdown();

                bool IsRayQueryAvailable() const { return rayQueryAvailable; }
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

                void UpdateDDGI(GI::DDGIVolume& volume, int frameIndex);

                GpuId CreateDeviceLocalBuffer(const void* data, std::size_t size, VkBufferUsageFlags extraUsage);
                std::uint64_t GetBufferDeviceAddress(GpuId buffer) const;

                GpuId CreateComputeProgram(const std::string& computeSource);
                void DestroyComputeProgram(GpuId program);
                VkPipeline GetComputePipeline(GpuId program) const;

                GpuId CreateStorageImage2D(int width, int height, TextureFormat format);
                void BindStorageImage2D(GpuId texture, unsigned int binding, StorageAccess access);

                GpuId CreateStorageBuffer(std::size_t size);
                void UpdateStorageBuffer(GpuId buffer, const void* data, std::size_t size, std::size_t offset);
                void BindStorageBuffer(GpuId buffer, unsigned int binding);

                void MemoryBarrierComputeToGraphics();

                void RebuildAccelerationStructures(GI::RayTracingScene& rtScene, Scene::Scene* scene);

            private:
                struct DeviceBuffer {
                    VkBuffer buffer = VK_NULL_HANDLE;
                    VkDeviceMemory memory = VK_NULL_HANDLE;
                    VkDeviceSize size = 0;
                    VkBufferUsageFlags usage = 0;
                };

                struct CachedBlas {
                    std::size_t geometrySignature = 0;
                    VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
                    VkBuffer blasBuffer = VK_NULL_HANDLE;
                    VkDeviceMemory blasMemory = VK_NULL_HANDLE;
                    DeviceBuffer vertices{};
                    DeviceBuffer indices{};
                    VkDeviceAddress blasDeviceAddress = 0;
                    bool built = false;
                };

                struct FrameBlasBuild {
                    Mesh* mesh = nullptr;
                    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
                    VkAccelerationStructureGeometryKHR geometry{};
                    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
                    VkBuffer scratchBuffer = VK_NULL_HANDLE;
                    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
                    VkBuffer vertexStaging = VK_NULL_HANDLE;
                    VkDeviceMemory vertexStagingMemory = VK_NULL_HANDLE;
                    VkBuffer indexStaging = VK_NULL_HANDLE;
                    VkDeviceMemory indexStagingMemory = VK_NULL_HANDLE;
                    VkDeviceSize vertexBytes = 0;
                    VkDeviceSize indexBytes = 0;
                };

                struct FrameTlasBuild {
                    bool needed = false;
                    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
                    VkAccelerationStructureGeometryKHR geometry{};
                    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
                    VkBuffer scratchBuffer = VK_NULL_HANDLE;
                    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
                    VkBuffer instanceStaging = VK_NULL_HANDLE;
                    VkDeviceMemory instanceStagingMemory = VK_NULL_HANDLE;
                    VkDeviceSize instanceBytes = 0;
                };

                bool LoadRayQueryExtensions();
                bool CreateDDGIResources();
                void DestroyDDGIResources();
                void DestroyAccelerationStructures();
                void OrphanBlasEntry(CachedBlas& entry);
                void OrphanTlasResources();
                bool PrepareBlasBuild(Mesh* mesh, std::size_t geometrySignature, CachedBlas& entry, FrameBlasBuild& outBuild);
                bool PrepareTlasBuild(const std::vector<GI::RayTracingMeshInstance>& instances, FrameTlasBuild& outBuild);
                std::string LoadShaderFile(const char* relativePath) const;
                std::string PreprocessComputeShader(const std::string& source) const;
                VkShaderModule CompileComputeModule(const std::string& source) const;

                VulkanRenderDevice& deviceOwner;
                VkDevice device = VK_NULL_HANDLE;

                bool rayQueryAvailable = false;
                bool initialized = false;

                VkDescriptorSetLayout giDescLayout = VK_NULL_HANDLE;
                VkPipelineLayout giPipelineLayout = VK_NULL_HANDLE;
                VkPipeline ddgiTracePipeline = VK_NULL_HANDLE;
                VkDescriptorPool giDescPool = VK_NULL_HANDLE;
                VkDescriptorSet giDescSet = VK_NULL_HANDLE;

                VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
                VkBuffer tlasBuffer = VK_NULL_HANDLE;
                VkDeviceMemory tlasMemory = VK_NULL_HANDLE;
                VkBuffer tlasInstanceBuffer = VK_NULL_HANDLE;
                VkDeviceMemory tlasInstanceMemory = VK_NULL_HANDLE;

                std::unordered_map<Mesh*, CachedBlas> blasCache;

                GpuId ddgiParamsBuffer = kInvalidGpuId;

                std::unordered_map<GpuId, DeviceBuffer> deviceLocalBuffers;
                GpuId nextGiId = 100000;
                std::size_t cachedInstanceSignature = 0;
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
