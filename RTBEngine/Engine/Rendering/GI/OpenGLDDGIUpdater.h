#pragma once

#include "DDGIUpdater.h"
#include "../RHI/RenderTypes.h"

namespace RTBEngine {
    namespace Rendering {
        namespace GI {

            class OpenGLDDGIUpdater final : public DDGIUpdater {
            public:
                bool Initialize() override;
                void Shutdown() override;
                void Update(DDGIVolume& volume, RayTracingScene& rtScene, Scene::Scene* scene, int frameIndex) override;

            private:
                RHI::GpuId computeProgram = RHI::kInvalidGpuId;
                RHI::GpuId paramsUBO = RHI::kInvalidGpuId;
                RHI::GpuId depthCube = RHI::kInvalidGpuId;
                bool ready = false;
            };

        }
    }
}
