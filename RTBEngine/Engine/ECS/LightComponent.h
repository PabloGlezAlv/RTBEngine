#pragma once
#include "Component.h"
#include "../Rendering/Lighting/Light.h"

namespace RTBEngine {
    namespace ECS {

        class LightComponent : public Component {
        public:
            LightComponent(Rendering::Light* light);
            ~LightComponent();

            void OnAwake() override;
            void OnStart() override;
            void OnUpdate(float deltaTime) override;

            Rendering::Light* GetLight() const { return light; }
            void SetLight(Rendering::Light* light);

            const char* GetTypeName() const override { return "LightComponent"; }
        private:
            Rendering::Light* light;
        };

    }
}