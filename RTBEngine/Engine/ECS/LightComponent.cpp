#include "LightComponent.h"

namespace RTBEngine {
    namespace ECS {

        LightComponent::LightComponent(std::unique_ptr<Rendering::Light> light)
            : Component(), light(std::move(light))
        {
        }

        LightComponent::~LightComponent()
        {
            
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

        void LightComponent::SetLight(std::unique_ptr<Rendering::Light> light)
        {
            this->light = std::move(light);
        }

    }
}