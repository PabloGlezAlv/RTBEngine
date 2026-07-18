#pragma once

#include "../../RTBEngineAPI.h"
#include "IRenderDevice.h"
#include <memory>

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            // Global access to the active render device (set during Application::Initialize).
            class RTB_API RenderDevice {
            public:
                static void Set(std::unique_ptr<IRenderDevice> device);
                static IRenderDevice& Get();
                static IRenderDevice* TryGet();
                static bool HasDevice();
                static void Shutdown();

            private:
                RenderDevice() = delete;
            };

        }
    }
}
