#include "Texture.h"
#include "../../ThirdParty/stb/stb_image.h"
#include <iostream>
#include "../RTBEngine.h"

namespace RTBEngine {
    namespace Rendering {

        Texture::Texture()
            : textureID(0), width(0), height(0), channels(0)
        {
        }

        Texture::~Texture()
        {
            if (textureID != 0) {
                glDeleteTextures(1, &textureID);
            }
        }

        bool Texture::LoadFromFile(const std::string& path)
        {
            stbi_set_flip_vertically_on_load(true);

            unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
            if (!data) {
                RTB_ERROR("Failed to load texture: " + path);
                return false;
            }

            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);

            GLenum format = GL_RGB;
            if (channels == 1)
                format = GL_RED;
            else if (channels == 3)
                format = GL_RGB;
            else if (channels == 4)
                format = GL_RGBA;

            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

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

            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);

            GLenum format = GL_RGB;
            if (channels == 1)
                format = GL_RED;
            else if (channels == 3)
                format = GL_RGB;
            else if (channels == 4)
                format = GL_RGBA;

            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

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

            // Don't flip - embedded textures from FBX are already correctly oriented
            // (Assimp already flips UVs via aiProcess_FlipUVs)
            stbi_set_flip_vertically_on_load(false);

            // stbi_load_from_memory decodes PNG/JPG/etc from memory buffer
            unsigned char* pixels = stbi_load_from_memory(data, dataSize, &width, &height, &channels, 0);
            if (!pixels) {
                RTB_ERROR("Failed to decode compressed texture from memory");
                return false;
            }

            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);

            GLenum format = GL_RGB;
            if (channels == 1)
                format = GL_RED;
            else if (channels == 3)
                format = GL_RGB;
            else if (channels == 4)
                format = GL_RGBA;

            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
            glGenerateMipmap(GL_TEXTURE_2D);

            SetFilter(TextureFilter::Linear, TextureFilter::Linear);
            SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);

            stbi_image_free(pixels);

            return true;
        }

        bool Texture::CreateDepthTexture(int width, int height) {
            this->width = width;
            this->height = height;
            this->channels = 1;

            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0,
                GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

            SetDepthTextureParams();

            glBindTexture(GL_TEXTURE_2D, 0);

            return textureID != 0;
        }

        void Texture::SetDepthTextureParams() {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

            float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }

        void Texture::Bind(unsigned int slot) const
        {
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(GL_TEXTURE_2D, textureID);
        }

        void Texture::Unbind() const
        {
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        void Texture::SetFilter(TextureFilter minFilter, TextureFilter magFilter)
        {
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GetGLFilter(minFilter));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GetGLFilter(magFilter));
        }

        void Texture::SetWrap(TextureWrap wrapS, TextureWrap wrapT)
        {
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GetGLWrap(wrapS));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GetGLWrap(wrapT));
        }

        GLenum Texture::GetGLFilter(TextureFilter filter) const
        {
            switch (filter) {
            case TextureFilter::Nearest: return GL_NEAREST;
            case TextureFilter::Linear:  return GL_LINEAR;
            default: return GL_LINEAR;
            }
        }

        GLenum Texture::GetGLWrap(TextureWrap wrap) const
        {
            switch (wrap) {
            case TextureWrap::Repeat:         return GL_REPEAT;
            case TextureWrap::ClampToEdge:    return GL_CLAMP_TO_EDGE;
            case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
            default: return GL_REPEAT;
            }
        }

    }
}