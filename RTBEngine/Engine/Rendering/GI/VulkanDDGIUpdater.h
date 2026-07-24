#pragma once

#include "DDGIUpdater.h"

namespace RTBEngine {
    namespace Rendering {
        namespace GI {

            class VulkanDDGIUpdater final : public DDGIUpdater {
            public:
                bool Initialize() override;
                void Shutdown() override;
                void Update(DDGIVolume& volume, RayTracingScene& rtScene, Scene::Scene* scene, int frameIndex) override;
            };

        }
    }
}
