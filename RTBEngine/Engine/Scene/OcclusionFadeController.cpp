#include "OcclusionFadeController.h"

#include "Scene.h"
#include "SceneManager.h"

#include <algorithm>

namespace RTBEngine {
    namespace ECS {

        using ThisClass = OcclusionFadeController;
        RTB_REGISTER_COMPONENT(OcclusionFadeController)
            RTB_PROPERTY_RANGE(occludedAlpha, 0.05f, 1.0f)
            RTB_PROPERTY_RANGE(fadeSpeed, 1.0f, 30.0f)
            RTB_PROPERTY_RANGE(boundsPadding, 0.0f, 1.0f)
            RTB_PROPERTY(controllerEnabled)
        RTB_END_REGISTER(OcclusionFadeController)

        void OcclusionFadeController::OnLateUpdate(float deltaTime)
        {
            if (!IsEnabled()) {
                return;
            }

            Scene* scene = SceneManager::GetInstance().GetActiveScene();
            if (!scene) {
                return;
            }

            OcclusionFadeSystem::Update(scene, BuildSettings(), deltaTime);
        }

        void OcclusionFadeController::OnDestroy()
        {
            if (Scene* scene = SceneManager::GetInstance().GetActiveScene()) {
                OcclusionFadeSystem::Reset(scene);
            }
        }

        void OcclusionFadeController::OnValidate()
        {
            ClampSettings();
        }

        void OcclusionFadeController::ClampSettings()
        {
            occludedAlpha = std::clamp(occludedAlpha, 0.05f, 1.0f);
            fadeSpeed = std::max(1.0f, fadeSpeed);
            boundsPadding = std::max(0.0f, boundsPadding);
        }

        OcclusionFadeSettings OcclusionFadeController::BuildSettings() const
        {
            OcclusionFadeSettings settings;
            settings.occludedAlpha = occludedAlpha;
            settings.fadeSpeed = fadeSpeed;
            settings.boundsPadding = boundsPadding;
            settings.enabled = controllerEnabled && IsEnabled();
            return settings;
        }

    }
}
