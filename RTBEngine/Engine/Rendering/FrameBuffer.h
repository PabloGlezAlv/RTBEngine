#pragma once
#include "../RTBEngineAPI.h"
#include "RHI/RenderTypes.h"
#include <memory>

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
            // Shares the color attachment of `source` with no depth, so a later pass can
            // sample the source depth texture without a framebuffer feedback loop.
            bool SyncColorOnlyContinue(const Framebuffer& source);
            void Resize(int width, int height);
            void Bind() const;
            void Unbind() const;
            void AttachDepthTexture(unsigned int textureID);
            void AttachColorTexture(unsigned int textureID);
            bool IsComplete() const;

            // Lazily creates/updates a color-only companion that shares this FBO's color.
            Framebuffer* GetColorOnlyContinueTarget();

            unsigned int GetID() const { return fboID; }
            unsigned int GetColorTextureID() const { return colorTextureID; }
            unsigned int GetDepthTextureID() const { return depthTextureID; }
            int GetWidth() const { return width; }
            int GetHeight() const { return height; }
            bool OwnsColorTexture() const { return ownsColorTexture; }

        private:
            void CreateTextures();
            void DeleteTextures();

            RHI::GpuId fboID = RHI::kInvalidGpuId;
            RHI::GpuId colorTextureID = RHI::kInvalidGpuId;
            RHI::GpuId depthTextureID = RHI::kInvalidGpuId;
            int width = 0;
            int height = 0;
            bool ownsColorTexture = true;
            bool ownsDepthTexture = true;
            std::unique_ptr<Framebuffer> colorOnlyContinue;
        };

    }
}
