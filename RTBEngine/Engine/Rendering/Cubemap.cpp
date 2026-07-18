#include "Cubemap.h"
#include "RHI/RenderDevice.h"
#include <stb_image.h>
#include "../Core/Logger.h"
#include <array>

namespace RTBEngine {
    namespace Rendering {

        namespace {
            RHI::IRenderDevice& Device()
            {
                return RHI::RenderDevice::Get();
            }

            RHI::TextureFormat FormatFromChannels(int channels)
            {
                if (channels == 1) return RHI::TextureFormat::R8;
                if (channels == 4) return RHI::TextureFormat::RGBA8;
                return RHI::TextureFormat::RGB8;
            }
        }

        Cubemap::Cubemap() = default;

        Cubemap::~Cubemap() {
            if (textureID != RHI::kInvalidGpuId && RHI::RenderDevice::HasDevice()) {
                Device().DestroyTexture(textureID);
                textureID = RHI::kInvalidGpuId;
            }
        }

        bool Cubemap::LoadFromFolder(const std::string& folderPath, const std::string& extension) {
            std::array<std::string, 6> facePaths = {
                folderPath + "/right" + extension,
                folderPath + "/left" + extension,
                folderPath + "/top" + extension,
                folderPath + "/bottom" + extension,
                folderPath + "/front" + extension,
                folderPath + "/back" + extension
            };
            return LoadFromFiles(facePaths);
        }

        bool Cubemap::LoadFromFiles(const std::array<std::string, 6>& facePaths) {
            auto& device = Device();
            textureID = device.CreateCubemap();

            stbi_set_flip_vertically_on_load(false);

            for (int i = 0; i < 6; i++) {
                int width = 0, height = 0, channels = 0;
                unsigned char* data = stbi_load(facePaths[i].c_str(), &width, &height, &channels, 0);

                if (!data) {
                    RTB_ERROR("Failed to load cubemap face: " + facePaths[i]);
                    device.DestroyTexture(textureID);
                    textureID = RHI::kInvalidGpuId;
                    return false;
                }

                device.SetCubemapFace(textureID, i, FormatFromChannels(channels), width, height, data);
                stbi_image_free(data);
            }

            device.SetCubemapFilterWrap(textureID);
            return true;
        }

        bool Cubemap::CreateSolidColor(float r, float g, float b) {
            auto& device = Device();
            textureID = device.CreateCubemap();

            unsigned char color[3] = {
                static_cast<unsigned char>(r * 255),
                static_cast<unsigned char>(g * 255),
                static_cast<unsigned char>(b * 255)
            };

            for (int i = 0; i < 6; i++) {
                device.SetCubemapFace(textureID, i, RHI::TextureFormat::RGB8, 1, 1, color);
            }

            device.SetCubemapFilterWrap(textureID);
            return true;
        }

        void Cubemap::Bind(unsigned int slot) const {
            Device().BindCubemap(textureID, slot);
        }

        void Cubemap::Unbind() const {
            Device().UnbindTexture2D();
        }

    }
}
