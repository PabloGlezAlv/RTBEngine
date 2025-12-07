#include "Texture.h"
#include "../../ThirdParty/stb/stb_image.h"
#include <iostream>

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
                std::cerr << "Failed to load texture: " << path << std::endl;
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