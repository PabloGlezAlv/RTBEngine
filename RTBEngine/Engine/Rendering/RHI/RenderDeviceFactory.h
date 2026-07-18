#pragma once

#include "../../RTBEngineAPI.h"
#include "GraphicsAPI.h"
#include "IRenderDevice.h"
#include <memory>

struct SDL_Window;

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            class RTB_API RenderDeviceFactory {
            public:
                // Creates a device for the requested API. Vulkan falls back to OpenGL in Phase 1.
                static std::unique_ptr<IRenderDevice> Create(GraphicsAPI requestedAPI);
            };

        }
    }
}
