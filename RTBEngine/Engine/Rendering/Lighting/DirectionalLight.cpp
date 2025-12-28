#include "DirectionalLight.h"

namespace RTBEngine {
    namespace Rendering {

        DirectionalLight::DirectionalLight()
            : Light(LightType::Directional), direction(0.0f, -1.0f, 0.0f), castShadows(true), shadowBias(0.005f)
        {
        }


        DirectionalLight::DirectionalLight(const Math::Vector3& direction, const Math::Vector3& color)
            : Light(LightType::Directional), direction(direction.Normalized()), castShadows(true), shadowBias(0.005f)
        {
            this->color = color;
        }

        void DirectionalLight::ApplyToShader(Shader* shader)
        {
            if (shader) {
                shader->SetVector3("dirLight.direction", direction);
                shader->SetVector3("dirLight.color", color);
                shader->SetFloat("dirLight.intensity", intensity);
            }
        }

        void DirectionalLight::SetCastShadows(bool enabled) {
            castShadows = enabled;

            if (enabled && !shadowMap) {
                shadowMap = std::make_unique<ShadowMap>(2048);
                shadowMap->Initialize();
            }
        }

        void DirectionalLight::SetShadowMapResolution(int resolution) {
            shadowMap = std::make_unique<ShadowMap>(resolution);
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