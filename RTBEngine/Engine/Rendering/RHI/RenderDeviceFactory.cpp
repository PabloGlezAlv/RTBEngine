#include "RenderDeviceFactory.h"
#include "OpenGL/OpenGLRenderDevice.h"
#include "Vulkan/VulkanRenderDevice.h"
#include "../../Core/Logger.h"

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            std::unique_ptr<IRenderDevice> RenderDeviceFactory::Create(GraphicsAPI requestedAPI)
            {
                switch (requestedAPI) {
                case GraphicsAPI::Vulkan:
                    return std::make_unique<VulkanRenderDevice>();
                case GraphicsAPI::OpenGL:
                    return std::make_unique<OpenGLRenderDevice>();
                default:
                    RTB_ERROR("RenderDeviceFactory: unsupported GraphicsAPI, using OpenGL");
                    return std::make_unique<OpenGLRenderDevice>();
                }
            }

        }
    }
}
