#include "PointLight.h"
#include <string>

namespace RTBEngine {
    namespace Rendering {

        PointLight::PointLight()
            : Light(LightType::Point)
            , position(0, 0, 0)
            , constant(1.0f)
            , linear(0.09f)
            , quadratic(0.032f)
            , range(50.0f) {
        }

        PointLight::PointLight(const Math::Vector3& position, const Math::Vector3& color)
            : Light(LightType::Point)
            , position(position)
            , constant(1.0f)
            , linear(0.09f)
            , quadratic(0.032f)
            , range(50.0f) {
            this->color = color;
        }

        void PointLight::SetAttenuation(float constant, float linear, float quadratic) {
            this->constant = constant;
            this->linear = linear;
            this->quadratic = quadratic;
        }

        void PointLight::SetRange(float range) {
            this->range = range;
            // Auto-calculate attenuation based on range
            // Formula: at range distance, light should be ~1% intensity
            linear = 4.5f / range;
            quadratic = 75.0f / (range * range);
        }

        void PointLight::ApplyToShader(Shader* shader) {
            ApplyToShader(shader, 0);
        }

        void PointLight::ApplyToShader(Shader* shader, int index) {
            std::string base = "pointLights[" + std::to_string(index) + "].";

            shader->SetVector3((base + "position").c_str(), position);
            shader->SetVector3((base + "color").c_str(), color);
            shader->SetFloat((base + "intensity").c_str(), intensity);
            shader->SetFloat((base + "constant").c_str(), constant);
            shader->SetFloat((base + "linear").c_str(), linear);
            shader->SetFloat((base + "quadratic").c_str(), quadratic);
            shader->SetFloat((base + "range").c_str(), range);
        }

    }
}
