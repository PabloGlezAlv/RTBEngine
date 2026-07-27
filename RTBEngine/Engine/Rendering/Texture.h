#pragma once
#include "../RTBEngineAPI.h"
#include <string>
#include "RHI/RenderTypes.h"

namespace RTBEngine {
    namespace Rendering {

        enum class TextureFilter {
            Nearest,
            Linear
        };

        enum class TextureWrap {
            Repeat,
            ClampToEdge,
            MirroredRepeat
        };

        class RTB_API Texture {
        public:
            Texture();
            ~Texture();

            Texture(const Texture&) = delete;
            Texture& operator=(const Texture&) = delete;

            bool LoadFromFile(const std::string& path, bool flipVertically = true);

            // Load from raw RGBA data (width * height * channels bytes)
            bool LoadFromMemory(const unsigned char* data, int width, int height, int channels);

            // Load from compressed image data (PNG/JPG in memory)
            bool LoadFromCompressedMemory(const unsigned char* data, int dataSize);

            bool CreateDepthTexture(int width, int height);
            void SetDepthTextureParams();

            // When false, ~Texture will not call into the GPU device (used during
            // process/static teardown after Vulkan validation layers may already be gone).
            static void SetGpuDestroyEnabled(bool enabled);
            static bool IsGpuDestroyEnabled();

            void Bind(unsigned int slot = 0) const;
            void Unbind() const;

            void SetFilter(TextureFilter minFilter, TextureFilter magFilter);
            void SetWrap(TextureWrap wrapS, TextureWrap wrapT);

            int GetWidth() const { return width; }
            int GetHeight() const { return height; }
            int GetChannels() const { return channels; }
            unsigned int GetID() const { return textureID; }

        private:
            static RHI::TextureFilter ToRHIFilter(TextureFilter filter);
            static RHI::TextureWrap ToRHIWrap(TextureWrap wrap);
            static RHI::TextureFormat FormatFromChannels(int channels);

            RHI::GpuId textureID = RHI::kInvalidGpuId;
            int width = 0;
            int height = 0;
            int channels = 0;
        };

    }
}
