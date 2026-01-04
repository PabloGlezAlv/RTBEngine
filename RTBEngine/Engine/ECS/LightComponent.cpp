#include "LightComponent.h"
#include "GameObject.h"
#include "Transform.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "../Reflection/TypeInfo.h"

namespace RTBEngine {
    namespace ECS {

        // Property registration for reflection system
        using ThisClass = LightComponent;
        RTB_REGISTER_COMPONENT(LightComponent)
            RTB_PROPERTY_ENUM(type, "Directional", "Point", "Spot")
            RTB_PROPERTY_COLOR(color)
            RTB_PROPERTY(intensity)
            RTB_PROPERTY_RANGE(range, 0.0f, 1000.0f)
            RTB_PROPERTY_RANGE(spotAngle, 0.0f, 180.0f)
            RTB_PROPERTY_RANGE(spotInnerAngle, 0.0f, 180.0f)
            RTB_PROPERTY(syncPosition)
            RTB_PROPERTY(syncDirection)
        RTB_END_REGISTER(LightComponent)

        LightComponent::LightComponent()
            : Component()
            , light(std::make_unique<Rendering::PointLight>())
        {
            type = Rendering::LightType::Point;
        }

        LightComponent::LightComponent(std::unique_ptr<Rendering::Light> light)
            : Component()
            , light(std::move(light)) {
            if (this->light) {
                type = this->light->GetType();
                color = this->light->GetColor();
                intensity = this->light->GetIntensity();
            }
        }

        LightComponent::~LightComponent() {
        }

        void LightComponent::OnAwake() {
            SyncProperties();
            SyncWithTransform();
        }

        void LightComponent::OnStart() {
            SyncProperties();
            SyncWithTransform();
        }

        void LightComponent::OnUpdate(float deltaTime) {
            SyncProperties();
            SyncWithTransform();
        }

        void LightComponent::SetLight(std::unique_ptr<Rendering::Light> light) {
            this->light = std::move(light);
        }

        void LightComponent::SyncProperties() {
            if (!light || light->GetType() != type) {
                switch (type) {
                case Rendering::LightType::Directional:
                    light = std::make_unique<Rendering::DirectionalLight>();
                    break;
                case Rendering::LightType::Point:
                    light = std::make_unique<Rendering::PointLight>();
                    break;
                case Rendering::LightType::Spot:
                    light = std::make_unique<Rendering::SpotLight>();
                    break;
                }
            }

            if (light) {
                light->SetColor(color);
                light->SetIntensity(intensity);

                if (type == Rendering::LightType::Point) {
                    auto* pl = static_cast<Rendering::PointLight*>(light.get());
                    pl->SetRange(range);
                }
                else if (type == Rendering::LightType::Spot) {
                    auto* sl = static_cast<Rendering::SpotLight*>(light.get());
                    sl->SetRange(range);
                    sl->SetCutOff(spotInnerAngle, spotAngle);
                }
            }
        }

        void LightComponent::SyncWithTransform() {
            if (!light || !owner) return;

            if (syncPosition) {
                if (light->GetType() == Rendering::LightType::Point) {
                    auto* pointLight = static_cast<Rendering::PointLight*>(light.get());
                    pointLight->SetPosition(owner->GetWorldPosition());
                }
                else if (light->GetType() == Rendering::LightType::Spot) {
                    auto* spotLight = static_cast<Rendering::SpotLight*>(light.get());
                    spotLight->SetPosition(owner->GetWorldPosition());
                }
            }

            if (syncDirection) {
                if (light->GetType() == Rendering::LightType::Directional) {
                    auto* dirLight = static_cast<Rendering::DirectionalLight*>(light.get());
                    Math::Vector3 forward = owner->GetWorldRotation() * Math::Vector3(0, -1, 0);
                    dirLight->SetDirection(forward);
                }
                else if (light->GetType() == Rendering::LightType::Spot) {
                    auto* spotLight = static_cast<Rendering::SpotLight*>(light.get());
                    Math::Vector3 forward = owner->GetWorldRotation() * Math::Vector3(0, -1, 0);
                    spotLight->SetDirection(forward);
                }
            }
        }

    }
}
