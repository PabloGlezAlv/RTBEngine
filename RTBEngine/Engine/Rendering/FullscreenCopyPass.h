#pragma once

#include "../RTBEngineAPI.h"
#include "RHI/RenderTypes.h"

namespace RTBEngine {
    namespace Rendering {

        class Shader;

        class RTB_API FullscreenCopyPass {
        public:
            static FullscreenCopyPass& GetInstance();

            bool Initialize();
            void Shutdown();

            bool IsReady() const;
            void CopyToBoundTarget(RHI::GpuId sourceTexture);

        private:
            FullscreenCopyPass() = default;
            ~FullscreenCopyPass();

            FullscreenCopyPass(const FullscreenCopyPass&) = delete;
            FullscreenCopyPass& operator=(const FullscreenCopyPass&) = delete;

            void CreateFullscreenTriangle();
            void DestroyFullscreenTriangle();

            Shader* shader = nullptr;
            RHI::GpuId vao = RHI::kInvalidGpuId;
            RHI::GpuId vbo = RHI::kInvalidGpuId;
        };

    }
}
