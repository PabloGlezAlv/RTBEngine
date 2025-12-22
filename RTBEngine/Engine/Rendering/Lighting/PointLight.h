#pragma once
#include "Light.h"
#include "../Shader.h"

namespace RTBEngine {
    namespace Rendering {

        class PointLight : public Light {
        public:
            PointLight();
            PointLight(const Math::Vector3& position, const Math::Vector3& color = Math::Vector3(1, 1, 1));
            ~PointLight() = default;

            void SetPosition(const Math::Vector3& position) { this->position = position; }
            Math::Vector3 GetPosition() const { return position; }

            void SetAttenuation(float constant, float linear, float quadratic);
            float GetConstant() const { return constant; }
            float GetLinear() const { return linear; }
            float GetQuadratic() const { return quadratic; }

            void SetRange(float range);
            float GetRange() const { return range; }

            void ApplyToShader(Shader* shader, int index);
            void ApplyToShader(Shader* shader) override;

        private:
            Math::Vector3 position;

            float constant = 1.0f;
            float linear = 0.09f;
            float quadratic = 0.032f;
            float range = 50.0f;
        };

    }
}
