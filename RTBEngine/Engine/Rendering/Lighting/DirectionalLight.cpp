#include "DirectionalLight.h"

namespace RTBEngine {
    namespace Rendering {

        DirectionalLight::DirectionalLight()
            : Light(LightType::Directional), direction(0.0f, -1.0f, 0.0f)
        {
        }

        DirectionalLight::DirectionalLight(const Math::Vector3& direction, const Math::Vector3& color)
            : Light(LightType::Directional), direction(direction.Normalized())
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

    }
}