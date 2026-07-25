#pragma once

#include "../../RTBEngineAPI.h"
#include "FogFrameState.h"

namespace RTBEngine {
    namespace Scene {
        class Scene;
    }

    namespace Rendering {
        class Camera;

        class RTB_API VolumeStack {
        public:
            static FogFrameState Evaluate(const Camera* camera, const Scene::Scene* scene);
            static const FogFrameState& GetCurrentFrameState();
        };

    }
}
