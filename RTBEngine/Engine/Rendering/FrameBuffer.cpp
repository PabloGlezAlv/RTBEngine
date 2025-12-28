#include "Framebuffer.h"
#include <iostream>

namespace RTBEngine {
    namespace Rendering {

        Framebuffer::Framebuffer() : fboID(0) {}

        Framebuffer::~Framebuffer() {
            if (fboID != 0) {
                glDeleteFramebuffers(1, &fboID);
            }
        }

        bool Framebuffer::Create() {
            glGenFramebuffers(1, &fboID);
            return fboID != 0;
        }

        void Framebuffer::Bind() const {
            glBindFramebuffer(GL_FRAMEBUFFER, fboID);
        }

        void Framebuffer::Unbind() const {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        void Framebuffer::AttachDepthTexture(GLuint textureID) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textureID, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }

        bool Framebuffer::IsComplete() const {
            return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        }

    }
}
