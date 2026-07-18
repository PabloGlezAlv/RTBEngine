#include "RenderDevice.h"
#include "../../Core/Logger.h"

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            namespace {
                std::unique_ptr<IRenderDevice> g_device;
            }

            void RenderDevice::Set(std::unique_ptr<IRenderDevice> device)
            {
                g_device = std::move(device);
            }

            IRenderDevice& RenderDevice::Get()
            {
                if (!g_device) {
                    RTB_ERROR("RenderDevice::Get called before a device was created");
                }
                return *g_device;
            }

            IRenderDevice* RenderDevice::TryGet()
            {
                return g_device.get();
            }

            bool RenderDevice::HasDevice()
            {
                return g_device != nullptr;
            }

            void RenderDevice::Shutdown()
            {
                if (g_device) {
                    g_device->Shutdown();
                    g_device.reset();
                }
            }

        }
    }
}
