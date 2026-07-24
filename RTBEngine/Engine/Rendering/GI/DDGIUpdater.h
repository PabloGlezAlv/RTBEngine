#pragma once

#include "DDGIVolume.h"
#include "RayTracingScene.h"
#include "../../Scene/Scene.h"

namespace RTBEngine {
    namespace Rendering {
        namespace GI {

            class DDGIUpdater {
            public:
                virtual ~DDGIUpdater() = default;
                virtual bool Initialize() = 0;
                virtual void Shutdown() = 0;
                virtual void Update(DDGIVolume& volume, RayTracingScene& rtScene, Scene::Scene* scene, int frameIndex) = 0;
            };

        }
    }
}
