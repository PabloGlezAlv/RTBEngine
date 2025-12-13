#include "LightComponent.h"

namespace RTBEngine {
    namespace ECS {

        LightComponent::LightComponent(Rendering::Light* light)
            : Component(), light(light)
        {
        }

        LightComponent::~LightComponent()
        {
            if (light) {
                delete light;
                light = nullptr;
            }
        }

        void LightComponent::OnAwake()
        {
        }

        void LightComponent::OnStart()
        {
        }

        void LightComponent::OnUpdate(float deltaTime)
        {

        }

        void LightComponent::SetLight(Rendering::Light* light)
        {
            if (this->light) {
                delete this->light;
            }
            this->light = light;
        }

    }
}