#pragma once

#include "../RTBEngineAPI.h"

namespace RTBEngine {
    namespace Scene {

        class Scene;

        struct RTB_API OcclusionFadeSettings {
            float occludedAlpha = 0.35f;
            float fadeSpeed = 12.0f;
            float boundsPadding = 0.15f;
            bool enabled = true;
        };

        class RTB_API OcclusionFadeSystem {
        public:
            static void Update(Scene* scene, const OcclusionFadeSettings& settings, float deltaTime);
            static void Reset(Scene* scene);
        };

    }
}
