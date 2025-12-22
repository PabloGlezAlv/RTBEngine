#pragma once
#include "Light.h"
#include "../Shader.h"

namespace RTBEngine {
    namespace Rendering {

        class SpotLight : public Light {
        public:
            SpotLight();
            SpotLight(const Math::Vector3& position, const Math::Vector3& direction,
                const Math::Vector3& color = Math::Vector3(1, 1, 1));
            ~SpotLight() = default;

            void SetPosition(const Math::Vector3& position) { this->position = position; }
            Math::Vector3 GetPosition() const { return position; }

            void SetDirection(const Math::Vector3& direction) { this->direction = direction.Normalized(); }
            Math::Vector3 GetDirection() const { return direction; }

            // Cone angles (in degrees)
            void SetCutOff(float innerAngle, float outerAngle);
            float GetInnerCutOff() const { return innerCutOff; }
            float GetOuterCutOff() const { return outerCutOff; }

            // Attenuation
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
            Math::Vector3 direction;

            // Cone (stored as cosine for shader efficiency)
            float innerCutOff = 0.9763f;   // cos(12.5°)
            float outerCutOff = 0.9659f;   // cos(15°)

            // Attenuation
            float constant = 1.0f;
            float linear = 0.09f;
            float quadratic = 0.032f;
            float range = 50.0f;
        };

    }
}
