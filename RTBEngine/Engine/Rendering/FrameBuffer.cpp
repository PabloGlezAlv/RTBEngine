#include "Framebuffer.h"
#include <iostream>
#include "../RTBEngine.h"

namespace RTBEngine {
    namespace Rendering {

        Framebuffer::Framebuffer() {}

        Framebuffer::~Framebuffer() {
            DeleteTextures();
            if (fboID != 0) {
                glDeleteFramebuffers(1, &fboID);
            }
        }

        bool Framebuffer::Create() {
            glGenFramebuffers(1, &fboID);
            return fboID != 0;
        }

        bool Framebuffer::CreateWithColorAndDepth(int w, int h) {
            width = w;
            height = h;

            if (!Create()) {
                RTB_ERROR("Framebuffer: Failed to create FBO");
                return false;
            }

            Bind();
            CreateTextures();

            if (!IsComplete()) {
                RTB_ERROR("Framebuffer: Framebuffer is not complete");
                Unbind();
                return false;
            }

            Unbind();
            return true;
        }

        void Framebuffer::Resize(int w, int h) {
            if (width == w && height == h) return;

            width = w;
            height = h;

            DeleteTextures();

            Bind();
            CreateTextures();
            Unbind();
        }

        void Framebuffer::CreateTextures() {
            // Create color texture
            glGenTextures(1, &colorTextureID);
            glBindTexture(GL_TEXTURE_2D, colorTextureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTextureID, 0);

            // Create depth texture
            glGenTextures(1, &depthTextureID);
            glBindTexture(GL_TEXTURE_2D, depthTextureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTextureID, 0);

            glBindTexture(GL_TEXTURE_2D, 0);
        }

        void Framebuffer::DeleteTextures() {
            if (colorTextureID != 0) {
                glDeleteTextures(1, &colorTextureID);
                colorTextureID = 0;
            }
            if (depthTextureID != 0) {
                glDeleteTextures(1, &depthTextureID);
                depthTextureID = 0;
            }
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

        void Framebuffer::AttachColorTexture(GLuint textureID) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);
        }

        bool Framebuffer::IsComplete() const {
            return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        }

    }
}
