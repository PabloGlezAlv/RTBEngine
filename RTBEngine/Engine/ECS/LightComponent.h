#pragma once
#include "Component.h"
#include "../Rendering/Lighting/Light.h"
#include "../Rendering/Lighting/PointLight.h"
#include "../Rendering/Lighting/SpotLight.h"
#include "../Reflection/PropertyMacros.h"
#include <memory>

namespace RTBEngine {
    namespace ECS {

        class LightComponent : public Component {
        public:
            LightComponent();
            LightComponent(std::unique_ptr<Rendering::Light> light);
            ~LightComponent();

            void OnAwake() override;
            void OnStart() override;
            void OnUpdate(float deltaTime) override;

            Rendering::Light* GetLight() const { return light.get(); }
            void SetLight(std::unique_ptr<Rendering::Light> light);

            bool syncPosition = true;
            bool syncDirection = true;

            // Reflected properties
            Rendering::LightType type = Rendering::LightType::Point;
            Math::Vector3 color = Math::Vector3(1.0f);
            float intensity = 1.0f;
            float range = 10.0f;
            float spotAngle = 45.0f;
            float spotInnerAngle = 30.0f;

            RTB_COMPONENT(LightComponent)

        private:
            void SyncWithTransform();
            void SyncProperties();
            std::unique_ptr<Rendering::Light> light;
        };

    }
}
