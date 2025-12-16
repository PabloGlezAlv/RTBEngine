#pragma once
#include "Component.h"
#include "../Rendering/Lighting/Light.h"
#include <memory>

namespace RTBEngine {
    namespace ECS {

        class LightComponent : public Component {
        public:
            LightComponent(std::unique_ptr<Rendering::Light> light);
            ~LightComponent();

            void OnAwake() override;
            void OnStart() override;
            void OnUpdate(float deltaTime) override;

            Rendering::Light* GetLight() const { return light.get(); }
            void SetLight(std::unique_ptr<Rendering::Light> light);

            const char* GetTypeName() const override { return "LightComponent"; }
        private:
            std::unique_ptr<Rendering::Light> light;
        };

    }
}