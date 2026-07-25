#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Matrix/Matrix4.h"
#include "RHI/RenderTypes.h"

namespace RTBEngine {
    namespace Rendering {

        class Camera;
        class Shader;
        class DirectionalLight;

        // Fullscreen additive volumetric in-scattering (god rays) using scene depth
        // and the directional shadow map. Requires a readable depth texture.
        class RTB_API VolumetricFogPass {
        public:
            static VolumetricFogPass& GetInstance();

            bool Initialize(Shader* shader);
            void Shutdown();

            void Render(Camera* camera,
                        RHI::GpuId sceneDepthTexture,
                        DirectionalLight* shadowLight,
                        const Math::Matrix4& lightSpaceMatrix);

            bool IsReady() const { return shader != nullptr && vao != RHI::kInvalidGpuId; }

        private:
            VolumetricFogPass() = default;
            ~VolumetricFogPass();

            VolumetricFogPass(const VolumetricFogPass&) = delete;
            VolumetricFogPass& operator=(const VolumetricFogPass&) = delete;

            void CreateFullscreenTriangle();
            void DestroyFullscreenTriangle();

            Shader* shader = nullptr;
            RHI::GpuId vao = RHI::kInvalidGpuId;
            RHI::GpuId vbo = RHI::kInvalidGpuId;
        };

    }
}
