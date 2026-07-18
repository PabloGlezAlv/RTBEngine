#include "RenderDeviceFactory.h"
#include "OpenGL/OpenGLRenderDevice.h"
#include "../../Core/Logger.h"

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            std::unique_ptr<IRenderDevice> RenderDeviceFactory::Create(GraphicsAPI requestedAPI)
            {
                GraphicsAPI api = requestedAPI;
                if (api == GraphicsAPI::Vulkan) {
                    RTB_WARN("GraphicsAPI::Vulkan is not available yet (Phase 1). Falling back to OpenGL.");
                    api = GraphicsAPI::OpenGL;
                }

                if (api == GraphicsAPI::OpenGL) {
                    return std::make_unique<OpenGLRenderDevice>();
                }

                RTB_ERROR("RenderDeviceFactory: unsupported GraphicsAPI, using OpenGL");
                return std::make_unique<OpenGLRenderDevice>();
            }

        }
    }
}
