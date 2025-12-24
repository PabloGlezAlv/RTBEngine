#include "LightComponent.h"
#include "GameObject.h"
#include "Transform.h"

namespace RTBEngine {
    namespace ECS {
        LightComponent::LightComponent()
            : Component()
            , light(nullptr)
            , syncPosition(true)
            , syncDirection(true)
        {
        }

        LightComponent::LightComponent(std::unique_ptr<Rendering::Light> light)
            : Component()
            , light(std::move(light))
            , syncPosition(true)
            , syncDirection(true) {
        }

        LightComponent::~LightComponent() {
        }

        void LightComponent::OnAwake() {
            SyncWithTransform();
        }

        void LightComponent::OnStart() {
            SyncWithTransform();
        }

        void LightComponent::OnUpdate(float deltaTime) {
            SyncWithTransform();
        }

        void LightComponent::SetLight(std::unique_ptr<Rendering::Light> light) {
            this->light = std::move(light);
        }

        void LightComponent::SyncWithTransform() {
            if (!light || !owner) return;

            Transform& transform = owner->GetTransform();

            if (syncPosition) {
                if (light->GetType() == Rendering::LightType::Point) {
                    auto* pointLight = static_cast<Rendering::PointLight*>(light.get());
                    pointLight->SetPosition(transform.GetPosition());
                }
                else if (light->GetType() == Rendering::LightType::Spot) {
                    auto* spotLight = static_cast<Rendering::SpotLight*>(light.get());
                    spotLight->SetPosition(transform.GetPosition());
                }
            }

            if (syncDirection && light->GetType() == Rendering::LightType::Spot) {
                auto* spotLight = static_cast<Rendering::SpotLight*>(light.get());
                Math::Vector3 forward = transform.GetRotation() * Math::Vector3(0, -1, 0);
                spotLight->SetDirection(forward);
            }
        }

    }
}
