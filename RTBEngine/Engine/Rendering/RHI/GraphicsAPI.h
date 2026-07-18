#pragma once

#include "../../RTBEngineAPI.h"

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            enum class GraphicsAPI {
                OpenGL = 0,
                Vulkan = 1
            };

            inline const char* GraphicsAPIToString(GraphicsAPI api)
            {
                switch (api) {
                case GraphicsAPI::OpenGL: return "OpenGL";
                case GraphicsAPI::Vulkan: return "Vulkan";
                default: return "Unknown";
                }
            }

        }
    }
}
