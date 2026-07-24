#include "DirectionalLight.h"
#include "LightingProjectSettings.h"

namespace RTBEngine {
    namespace Rendering {

        namespace {
            int DefaultShadowResolution()
            {
                return LightingProjectSettings::Get().GetClampedShadowMapResolution();
            }
        }

        DirectionalLight::DirectionalLight()
            : Light(LightType::Directional), direction(0.0f, -1.0f, 0.0f), castShadows(true), shadowBias(0.005f)
        {
            shadowMap = std::make_unique<ShadowMap>(DefaultShadowResolution());
            shadowMap->Initialize();
        }


        DirectionalLight::DirectionalLight(const Math::Vector3& direction, const Math::Vector3& color)
            : Light(LightType::Directional), direction(direction.Normalized()), castShadows(true), shadowBias(0.005f)
        {
            this->color = color;
            shadowMap = std::make_unique<ShadowMap>(DefaultShadowResolution());
            shadowMap->Initialize();
        }

        void DirectionalLight::SetCastShadows(bool enabled) {
            castShadows = enabled;

            if (enabled && !shadowMap) {
                shadowMap = std::make_unique<ShadowMap>(DefaultShadowResolution());
                shadowMap->Initialize();
            }
        }

        void DirectionalLight::SetShadowMapResolution(int resolution) {
            const int clamped = LightingProjectSettings::ClampShadowMapResolution(resolution);
            if (shadowMap && shadowMap->GetResolution() == clamped) {
                return;
            }
            shadowMap = std::make_unique<ShadowMap>(clamped);
            shadowMap->Initialize();
        }

        int DirectionalLight::GetShadowMapResolution() const {
            return shadowMap ? shadowMap->GetResolution() : 0;
        }

        Math::Matrix4 DirectionalLight::GetLightSpaceMatrix(const Math::Vector3& sceneCenter, float sceneRadius) const {
            Math::Vector3 lightPos = sceneCenter - direction.Normalized() * sceneRadius;

            Math::Vector3 up(0.0f, 1.0f, 0.0f);
            if (abs(direction.y) > 0.99f) {
                up = Math::Vector3(1.0f, 0.0f, 0.0f);
            }

            Math::Matrix4 lightView = Math::Matrix4::LookAt(lightPos, sceneCenter, up);

            float orthoSize = sceneRadius;
            Math::Matrix4 lightProjection = Math::Matrix4::Orthographic(
                -orthoSize, orthoSize,
                -orthoSize, orthoSize,
                0.1f, sceneRadius * 3.0f
            );

            return lightProjection * lightView;
        }

    }
}
