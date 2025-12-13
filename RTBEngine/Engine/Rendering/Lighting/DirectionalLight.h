#pragma once
#include "Light.h"
#include "../Shader.h"

namespace RTBEngine {
    namespace Rendering {

        class DirectionalLight : public Light {
        public:
            DirectionalLight();
            DirectionalLight(const Math::Vector3& direction, const Math::Vector3& color = Math::Vector3(1, 1, 1));
            ~DirectionalLight() = default;

            void SetDirection(const Math::Vector3& direction) { this->direction = direction.Normalized(); }
            Math::Vector3 GetDirection() const { return direction; }

            void ApplyToShader(Shader* shader) override;

        private:
            Math::Vector3 direction;
        };

    }
}