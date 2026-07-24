#pragma once

#include "DDGIUpdater.h"
#include "../RHI/RenderTypes.h"
#include <cstddef>
#include <vector>

namespace RTBEngine {
    namespace Rendering {
        namespace GI {

            class OpenGLDDGIUpdater final : public DDGIUpdater {
            public:
                bool Initialize() override;
                void Shutdown() override;
                void Update(DDGIVolume& volume, RayTracingScene& rtScene, Scene::Scene* scene, int frameIndex) override;

            private:
                void EnsureTriangleBuffer(std::size_t floatCount);
                void UploadSceneTriangles(Scene::Scene* scene);

                RHI::GpuId computeProgram = RHI::kInvalidGpuId;
                RHI::GpuId paramsUBO = RHI::kInvalidGpuId;
                RHI::GpuId triangleSSBO = RHI::kInvalidGpuId;
                std::size_t triangleBufferCapacityFloats = 0;
                bool ready = false;
            };

        }
    }
}
