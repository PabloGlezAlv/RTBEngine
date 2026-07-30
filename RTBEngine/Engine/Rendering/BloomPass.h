#pragma once

#include "../RTBEngineAPI.h"
#include "RHI/RenderTypes.h"

namespace RTBEngine {
    namespace Rendering {

        class Shader;

        // HDR bloom: bright extract, separable blur, Reinhard-tonemapped composite.
        class RTB_API BloomPass {
        public:
            static BloomPass& GetInstance();

            bool Initialize();
            void Shutdown();

            void Render(RHI::GpuId sceneColorTexture, int width, int height);
            bool RenderIfReady(RHI::GpuId sceneColorTexture, int width, int height);

            bool IsReady() const;

            float threshold = 1.0f;
            float intensity = 0.8f;

        private:
            BloomPass() = default;
            ~BloomPass();

            BloomPass(const BloomPass&) = delete;
            BloomPass& operator=(const BloomPass&) = delete;

            void CreateFullscreenTriangle();
            void DestroyFullscreenTriangle();
            void DestroyRenderTargets();
            bool EnsureRenderTargets(int width, int height);
            bool EnsureOutputFramebuffer(RHI::GpuId sceneColorTexture, int width, int height);
            void DrawFullscreen(Shader* shader);

            Shader* extractShader = nullptr;
            Shader* blurShader = nullptr;
            Shader* compositeShader = nullptr;
            Shader* copyShader = nullptr;

            RHI::GpuId vao = RHI::kInvalidGpuId;
            RHI::GpuId vbo = RHI::kInvalidGpuId;

            RHI::GpuId brightFbo = RHI::kInvalidGpuId;
            RHI::GpuId brightTexture = RHI::kInvalidGpuId;
            RHI::GpuId pingFbo = RHI::kInvalidGpuId;
            RHI::GpuId pingTexture = RHI::kInvalidGpuId;
            RHI::GpuId pongFbo = RHI::kInvalidGpuId;
            RHI::GpuId pongTexture = RHI::kInvalidGpuId;
            RHI::GpuId scratchFbo = RHI::kInvalidGpuId;
            RHI::GpuId scratchTexture = RHI::kInvalidGpuId;
            RHI::GpuId outputFbo = RHI::kInvalidGpuId;
            RHI::GpuId outputColorTexture = RHI::kInvalidGpuId;

            int targetWidth = 0;
            int targetHeight = 0;
            int bloomWidth = 0;
            int bloomHeight = 0;
        };

    }
}
