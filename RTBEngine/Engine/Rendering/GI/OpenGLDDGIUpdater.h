#pragma once

#include "DDGIUpdater.h"
#include "../RHI/RenderTypes.h"
#include <cstddef>
#include <cstdint>

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
                // Returns false if upload was skipped because the scene signature matches cache.
                bool UploadSceneTriangles(Scene::Scene* scene);

                RHI::GpuId computeProgram = RHI::kInvalidGpuId;
                RHI::GpuId paramsUBO = RHI::kInvalidGpuId;
                RHI::GpuId triangleSSBO = RHI::kInvalidGpuId;
                std::size_t triangleBufferCapacityFloats = 0;
                std::uint64_t cachedSceneSignature = 0;
                bool hasCachedScene = false;
                bool ready = false;
            };

        }
    }
}
