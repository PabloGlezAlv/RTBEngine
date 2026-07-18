#pragma once
#include "../RTBEngineAPI.h"
#include "RHI/RenderTypes.h"

namespace RTBEngine {
    namespace Rendering {

        class RTB_API Framebuffer {
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
            void AttachDepthTexture(unsigned int textureID);
            void AttachColorTexture(unsigned int textureID);
            bool IsComplete() const;

            unsigned int GetID() const { return fboID; }
            unsigned int GetColorTextureID() const { return colorTextureID; }
            unsigned int GetDepthTextureID() const { return depthTextureID; }
            int GetWidth() const { return width; }
            int GetHeight() const { return height; }

        private:
            void CreateTextures();
            void DeleteTextures();

            RHI::GpuId fboID = RHI::kInvalidGpuId;
            RHI::GpuId colorTextureID = RHI::kInvalidGpuId;
            RHI::GpuId depthTextureID = RHI::kInvalidGpuId;
            int width = 0;
            int height = 0;
        };

    }
}
