#pragma once
#include "Light.h"
#include "../Shader.h"
#include "../ShadowMap.h"

#include <memory>

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


            void SetCastShadows(bool enabled);
            bool GetCastShadows() const { return castShadows; }

            void SetShadowMapResolution(int resolution);
            int GetShadowMapResolution() const;

            void SetShadowBias(float bias) { shadowBias = bias; }
            float GetShadowBias() const { return shadowBias; }

            Math::Matrix4 GetLightSpaceMatrix(const Math::Vector3& sceneCenter, float sceneRadius) const;

            ShadowMap* GetShadowMap() { return shadowMap.get(); }

        private:
            Math::Vector3 direction;

            bool castShadows;
            float shadowBias;
            std::unique_ptr<ShadowMap> shadowMap;

        };

    }
}