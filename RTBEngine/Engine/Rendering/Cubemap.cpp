#include "Cubemap.h"
#include <stb_image.h>
#include <iostream>
#include <array>

namespace RTBEngine {
    namespace Rendering {

        Cubemap::Cubemap() : textureID(0) {
        }

        Cubemap::~Cubemap() {
            if (textureID != 0) {
                glDeleteTextures(1, &textureID);
            }
        }

        bool Cubemap::LoadFromFolder(const std::string& folderPath, const std::string& extension) {
            
            std::array<std::string, 6> facePaths = {
                folderPath + "/right" + extension,   // +X
                folderPath + "/left" + extension,    // -X
                folderPath + "/top" + extension,     // +Y
                folderPath + "/bottom" + extension,  // -Y
                folderPath + "/front" + extension,   // +Z
                folderPath + "/back" + extension     // -Z
            };
            return LoadFromFiles(facePaths);
        }

        bool Cubemap::LoadFromFiles(const std::array<std::string, 6>& facePaths) {
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

            GLenum targets[6] = {
                GL_TEXTURE_CUBE_MAP_POSITIVE_X,  // right
                GL_TEXTURE_CUBE_MAP_NEGATIVE_X,  // left
                GL_TEXTURE_CUBE_MAP_POSITIVE_Y,  // top
                GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,  // bottom
                GL_TEXTURE_CUBE_MAP_POSITIVE_Z,  // front
                GL_TEXTURE_CUBE_MAP_NEGATIVE_Z   // back
            };

            stbi_set_flip_vertically_on_load(false);

            for (int i = 0; i < 6; i++) {
                int width, height, channels;
                unsigned char* data = stbi_load(facePaths[i].c_str(), &width, &height, &channels, 0);

                if (!data) {
                    std::cerr << "Failed to load cubemap face: " << facePaths[i] << std::endl;
                    glDeleteTextures(1, &textureID);
                    textureID = 0;
                    return false;
                }
                
                GLenum format = GL_RGB;
                if (channels == 1) format = GL_RED;
                else if (channels == 3) format = GL_RGB;
                else if (channels == 4) format = GL_RGBA;


                glTexImage2D(targets[i], 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

                stbi_image_free(data);
            }

            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            return true;
        }

        bool Cubemap::CreateSolidColor(float r, float g, float b) {
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

            unsigned char color[3] = {
                static_cast<unsigned char>(r * 255),
                static_cast<unsigned char>(g * 255),
                static_cast<unsigned char>(b * 255)
            };

            for (int i = 0; i < 6; i++) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, color);
            }

            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            return true;
        }

        void Cubemap::Bind(unsigned int slot) const {
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
        }

        void Cubemap::Unbind() const {
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        }

    }
}
