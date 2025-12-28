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
            void Bind() const;
            void Unbind() const;
            void AttachDepthTexture(GLuint textureID);
            bool IsComplete() const;

            GLuint GetID() const { return fboID; }

        private:
            GLuint fboID;
        };

    }
}
