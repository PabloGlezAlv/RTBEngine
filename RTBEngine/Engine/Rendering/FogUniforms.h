#pragma once

#include "../RTBEngineAPI.h"

namespace RTBEngine {
    namespace Rendering {

        struct FogFrameState;

        class Shader;
        class Camera;

        // Uploads distance/height + volumetric fog params from the current volume stack
        // (or project defaults if Evaluate has not run this frame).
        class RTB_API FogUniforms {
        public:
            static void Apply(Shader* shader);
            static void Apply(Shader* shader, const FogFrameState& state);
            static void ApplyCameraPlanes(Shader* shader, const Camera* camera);
        };

    }
}
