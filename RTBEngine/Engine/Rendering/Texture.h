#pragma once
#include <GL/glew.h>
#include <string>

namespace RTBEngine {
	namespace Rendering {
        class Texture {
        public:
            Texture();
            ~Texture();

            bool LoadFromFile(const std::string& path);
            void Bind(unsigned int slot = 0) const;
            void Unbind() const;

            unsigned int GetWidth() const;
            unsigned int GetHeight() const;
            GLuint GetID() const;
        private:
            GLuint textureID;
            int width, height, channels;
        };
	}
}
