#pragma once
#include <GL/glew.h>
#include <string>

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

        class Texture {
        public:
            Texture();
            ~Texture();

            Texture(const Texture&) = delete;
            Texture& operator=(const Texture&) = delete;

            bool LoadFromFile(const std::string& path);

            // Load from raw RGBA data (width * height * channels bytes)
            bool LoadFromMemory(const unsigned char* data, int width, int height, int channels);

            // Load from compressed image data (PNG/JPG in memory)
            bool LoadFromCompressedMemory(const unsigned char* data, int dataSize);

            bool CreateDepthTexture(int width, int height);
            void SetDepthTextureParams();

            void Bind(unsigned int slot = 0) const;
            void Unbind() const;

            void SetFilter(TextureFilter minFilter, TextureFilter magFilter);
            void SetWrap(TextureWrap wrapS, TextureWrap wrapT);

            int GetWidth() const { return width; }
            int GetHeight() const { return height; }
            int GetChannels() const { return channels; }
            GLuint GetID() const { return textureID; }

        private:
            GLenum GetGLFilter(TextureFilter filter) const;
            GLenum GetGLWrap(TextureWrap wrap) const;

            GLuint textureID;
            int width;
            int height;
            int channels;
        };

    }
}