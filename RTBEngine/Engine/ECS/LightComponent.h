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

            RTB_COMPONENT(LightComponent)

        private:
            void SyncWithTransform();
            std::unique_ptr<Rendering::Light> light;
        };

    }
}
