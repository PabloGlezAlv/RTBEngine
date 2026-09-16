#pragma once

#include "../../RTBEngineAPI.h"
#include <cctype>
#include <string>

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

            inline GraphicsAPI ParseGraphicsAPI(const std::string& value)
            {
                std::string lowered;
                lowered.reserve(value.size());
                for (unsigned char character : value) {
                    if (std::isspace(character) != 0) {
                        continue;
                    }
                    lowered.push_back(static_cast<char>(std::tolower(character)));
                }

                if (lowered == "vulkan" || lowered == "vk") {
                    return GraphicsAPI::Vulkan;
                }

                return GraphicsAPI::OpenGL;
            }

        }
    }
}
