#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "OcclusionFadeSystem.h"
#include "../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace Scene {

        class RTB_API OcclusionFadeController : public Component {
        public:
            OcclusionFadeController() = default;
            ~OcclusionFadeController() override = default;

            float occludedAlpha = 0.35f;
            float fadeSpeed = 12.0f;
            float boundsPadding = 0.15f;
            bool controllerEnabled = true;

            RTB_COMPONENT(OcclusionFadeController)

        public:
            void OnLateUpdate(float deltaTime) override;
            void OnDestroy() override;
            void OnValidate() override;

        private:
            void ClampSettings();
            OcclusionFadeSettings BuildSettings() const;
        };

    }
}
