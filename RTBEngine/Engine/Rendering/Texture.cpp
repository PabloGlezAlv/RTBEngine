#include "Texture.h"
#include "RHI/RenderDevice.h"
#include "../../ThirdParty/stb/stb_image.h"
#include "../Core/Logger.h"

namespace RTBEngine {
    namespace Rendering {

        namespace {
            RHI::IRenderDevice& Device()
            {
                return RHI::RenderDevice::Get();
            }
        }

        Texture::Texture() = default;

        Texture::~Texture()
        {
            if (textureID != RHI::kInvalidGpuId) {
                Device().DestroyTexture(textureID);
                textureID = RHI::kInvalidGpuId;
            }
        }

        RHI::TextureFilter Texture::ToRHIFilter(TextureFilter filter)
        {
            return filter == TextureFilter::Nearest ? RHI::TextureFilter::Nearest : RHI::TextureFilter::Linear;
        }

        RHI::TextureWrap Texture::ToRHIWrap(TextureWrap wrap)
        {
            switch (wrap) {
            case TextureWrap::ClampToEdge: return RHI::TextureWrap::ClampToEdge;
            case TextureWrap::MirroredRepeat: return RHI::TextureWrap::MirroredRepeat;
            case TextureWrap::Repeat:
            default: return RHI::TextureWrap::Repeat;
            }
        }

        RHI::TextureFormat Texture::FormatFromChannels(int channels)
        {
            if (channels == 1) return RHI::TextureFormat::R8;
            if (channels == 3) return RHI::TextureFormat::SRGB8;
            return RHI::TextureFormat::SRGBA8;
        }

        bool Texture::LoadFromFile(const std::string& path, bool flipVertically)
        {
            stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

            unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
            if (!data) {
                RTB_ERROR("Failed to load texture: " + path);
                return false;
            }

            textureID = Device().CreateTexture2D();
            Device().SetTexture2DData(textureID, FormatFromChannels(channels), width, height, data, true);
            SetFilter(TextureFilter::Linear, TextureFilter::Linear);
            SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);

            stbi_image_free(data);
            return true;
        }

        bool Texture::LoadFromMemory(const unsigned char* data, int w, int h, int ch)
        {
            if (!data || w <= 0 || h <= 0 || ch <= 0) {
                RTB_ERROR("Invalid texture data for LoadFromMemory");
                return false;
            }

            width = w;
            height = h;
            channels = ch;

            textureID = Device().CreateTexture2D();
            Device().SetTexture2DData(textureID, FormatFromChannels(channels), width, height, data, true);
            SetFilter(TextureFilter::Linear, TextureFilter::Linear);
            SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
            return true;
        }

        bool Texture::LoadFromCompressedMemory(const unsigned char* data, int dataSize)
        {
            if (!data || dataSize <= 0) {
                RTB_ERROR("Invalid compressed texture data");
                return false;
            }

            stbi_set_flip_vertically_on_load(false);

            unsigned char* pixels = stbi_load_from_memory(data, dataSize, &width, &height, &channels, 0);
            if (!pixels) {
                RTB_ERROR("Failed to decode compressed texture from memory");
                return false;
            }

            textureID = Device().CreateTexture2D();
            Device().SetTexture2DData(textureID, FormatFromChannels(channels), width, height, pixels, true);
            SetFilter(TextureFilter::Linear, TextureFilter::Linear);
            SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);

            stbi_image_free(pixels);
            return true;
        }

        bool Texture::CreateDepthTexture(int width, int height) {
            this->width = width;
            this->height = height;
            this->channels = 1;

            textureID = Device().CreateTexture2D();
            Device().SetTexture2DData(textureID, RHI::TextureFormat::Depth32F, width, height, nullptr, false);
            SetDepthTextureParams();
            return textureID != RHI::kInvalidGpuId;
        }

        void Texture::SetDepthTextureParams() {
            Device().SetTexture2DDepthShadowParams(textureID);
        }

        void Texture::Bind(unsigned int slot) const
        {
            Device().BindTexture2D(textureID, slot);
        }

        void Texture::Unbind() const
        {
            Device().UnbindTexture2D();
        }

        void Texture::SetFilter(TextureFilter minFilter, TextureFilter magFilter)
        {
            Device().SetTexture2DFilter(textureID, ToRHIFilter(minFilter), ToRHIFilter(magFilter));
        }

        void Texture::SetWrap(TextureWrap wrapS, TextureWrap wrapT)
        {
            Device().SetTexture2DWrap(textureID, ToRHIWrap(wrapS), ToRHIWrap(wrapT));
        }

    }
}
