#pragma once
#include <GL/glew.h>

namespace RTBEngine {
    namespace Rendering {

        class Framebuffer {
        public:
            Framebuffer();
            ~Framebuffer();

            Framebuffer(const Framebuffer&) = delete;
            Framebuffer& operator=(const Framebuffer&) = delete;

            bool Create();
            bool CreateWithColorAndDepth(int width, int height);
            void Resize(int width, int height);
            void Bind() const;
            void Unbind() const;
            void AttachDepthTexture(GLuint textureID);
            void AttachColorTexture(GLuint textureID);
            bool IsComplete() const;

            GLuint GetID() const { return fboID; }
            GLuint GetColorTextureID() const { return colorTextureID; }
            GLuint GetDepthTextureID() const { return depthTextureID; }
            int GetWidth() const { return width; }
            int GetHeight() const { return height; }

        private:
            void CreateTextures();
            void DeleteTextures();

            GLuint fboID = 0;
            GLuint colorTextureID = 0;
            GLuint depthTextureID = 0;
            int width = 0;
            int height = 0;
        };

    }
}
