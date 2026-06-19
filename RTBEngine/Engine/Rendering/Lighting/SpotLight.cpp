#include "SpotLight.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace RTBEngine {
    namespace Rendering {

        SpotLight::SpotLight()
            : Light(LightType::Spot)
            , position(0, 0, 0)
            , direction(0, -1, 0)
            , innerCutOff(std::cos(12.5f * M_PI / 180.0f))
            , outerCutOff(std::cos(15.0f * M_PI / 180.0f))
            , constant(1.0f)
            , linear(0.09f)
            , quadratic(0.032f)
            , range(50.0f) {
        }

        SpotLight::SpotLight(const Math::Vector3& position, const Math::Vector3& direction, const Math::Vector3& color)
            : Light(LightType::Spot)
            , position(position)
            , direction(direction.Normalized())
            , innerCutOff(std::cos(12.5f * M_PI / 180.0f))
            , outerCutOff(std::cos(15.0f * M_PI / 180.0f))
            , constant(1.0f)
            , linear(0.09f)
            , quadratic(0.032f)
            , range(50.0f) {
            this->color = color;
        }

        void SpotLight::SetCutOff(float innerAngle, float outerAngle) {
            // Convert degrees to radians, then to cosine
            innerCutOff = std::cos(innerAngle * M_PI / 180.0f);
            outerCutOff = std::cos(outerAngle * M_PI / 180.0f);
        }

        void SpotLight::SetAttenuation(float constant, float linear, float quadratic) {
            this->constant = constant;
            this->linear = linear;
            this->quadratic = quadratic;
        }

        void SpotLight::SetRange(float range) {
            this->range = range;
            linear = 4.5f / range;
            quadratic = 75.0f / (range * range);
        }

    }
}
