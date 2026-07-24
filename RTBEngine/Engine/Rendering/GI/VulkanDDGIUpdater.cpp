#include "VulkanDDGIUpdater.h"
#include "../RHI/RenderDevice.h"
#include "../Lighting/LightingUBO.h"
#include "../RHI/Vulkan/VulkanGiContext.h"
#include "../RHI/Vulkan/VulkanRenderDevice.h"
#include "../RHI/GraphicsAPI.h"

namespace RTBEngine {
    namespace Rendering {
        namespace GI {

            bool VulkanDDGIUpdater::Initialize()
            {
                return RHI::RenderDevice::Get().GetGiCapabilities().rayQuery;
            }

            void VulkanDDGIUpdater::Shutdown() {}

            void VulkanDDGIUpdater::Update(DDGIVolume& volume, RayTracingScene& rtScene, Scene::Scene* scene, int frameIndex)
            {
                if (RHI::RenderDevice::Get().GetAPI() != RHI::GraphicsAPI::Vulkan) return;
                auto& vkDevice = static_cast<RHI::VulkanRenderDevice&>(RHI::RenderDevice::Get());
                RHI::VulkanGiContext* gi = vkDevice.GetGiContext();
                if (!gi || !gi->IsRayQueryAvailable()) return;
                if (!gi->HasTracePipeline()) {
                    if (!gi->EnsureTracePipeline()) {
                        return;
                    }
                }

                gi->RebuildAccelerationStructures(rtScene, scene);
                gi->UpdateDDGI(volume, rtScene, scene, frameIndex);
                gi->MemoryBarrierComputeToGraphics();
            }

        }
    }
}
