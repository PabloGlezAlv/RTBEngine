#pragma once
#include "../../Math/Math.h"

namespace RTBEngine {
    namespace Rendering {

        enum class LightType {
            Directional,
            Point,
            Spot
        };

        class Light {
        public:
            Light(LightType type);
            virtual ~Light() = default;

            LightType GetType() const { return type; }

            void SetColor(const Math::Vector3& color) { this->color = color; }
            Math::Vector3 GetColor() const { return color; }

            void SetIntensity(float intensity) { this->intensity = intensity; }
            float GetIntensity() const { return intensity; }

            virtual void ApplyToShader(class Shader* shader) = 0;

        protected:
            LightType type;
            Math::Vector3 color;
            float intensity;
        };

    }
}