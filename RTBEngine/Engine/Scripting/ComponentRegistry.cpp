#include "ComponentRegistry.h"
#include <iostream>
#include "../RTBEngine.h"
#include "../Reflection/TypeInfo.h"

#include "../ECS/NetworkIdentity.h"
#include "../ECS/NetworkTransform.h"
#include "../Online/OnlineGameplayNet.h"
#include "../ECS/MeshRenderer.h"
#include "../ECS/LightComponent.h"
#include "../ECS/AudioSourceComponent.h"
#include "../ECS/RigidBodyComponent.h"
#include "../ECS/BoxColliderComponent.h"
#include "../ECS/SphereColliderComponent.h"
#include "../ECS/CapsuleColliderComponent.h"
#include "../ECS/NavGridComponent.h"
#include "../ECS/NavAgentComponent.h"
#include "../ECS/CameraComponent.h"
#include "../ECS/FreeLookCamera.h"
#include "../ECS/TrailRenderer.h"
#include "../Animation/Animator.h"
#include "../UI/Canvas.h"
#include "../UI/Elements/UIText.h"
#include "../UI/Elements/UIImage.h"
#include "../UI/Elements/UIPanel.h"
#include "../UI/Elements/UIButton.h"
#include "../UI/Elements/UIInputField.h"
#include "../UI/Elements/UIContainer.h"
#include "../UI/Elements/UIJoystick.h"
#include "../UI/Elements/UIHorizontalLayout.h"
#include "../UI/Elements/UIVerticalLayout.h"

namespace RTBEngine {
    namespace Scripting {

        ComponentRegistry& ComponentRegistry::GetInstance() {
            static ComponentRegistry instance;
            return instance;
        }

        void ComponentRegistry::RegisterComponent(const std::string& typeName,
            std::function<ECS::Component* ()> factory) {
            factories[typeName] = factory;
        }

        ECS::Component* ComponentRegistry::CreateComponent(const std::string& typeName) {
            auto it = factories.find(typeName);
            if (it != factories.end()) {
                return it->second();
            }

            // Fall back to TypeRegistry for script components registered via ScriptManager.
            const Reflection::TypeInfo* ti = GetComponentTypeInfo(typeName);
            if (ti) {
                return ti->Create();
            }

            RTB_ERROR("ComponentRegistry: Component type '" + typeName + "' not registered!");
            return nullptr;
        }

        const Reflection::TypeInfo* ComponentRegistry::GetComponentTypeInfo(const std::string& typeName) const {
            return Reflection::TypeRegistry::GetInstance().GetTypeInfo(typeName);
        }

        void ComponentRegistry::DestroyComponent(const std::string& typeName, ECS::Component* component) const {
            if (!component) {
                return;
            }

            const Reflection::TypeInfo* ti = GetComponentTypeInfo(typeName);
            if (ti) {
                ti->Destroy(component);
                return;
            }

            delete component;
        }

        bool ComponentRegistry::HasComponent(const std::string& typeName) const {
            if (factories.find(typeName) != factories.end()) return true;
            return Reflection::TypeRegistry::GetInstance().HasType(typeName);
        }

        void ComponentRegistry::RegisterBuiltInComponents() {
            RegisterComponent("MeshRenderer", []() { return new ECS::MeshRenderer(); });
            RegisterComponent("LightComponent", []() { return new ECS::LightComponent(); });
            RegisterComponent("AudioSourceComponent", []() { return new ECS::AudioSourceComponent(); });
            RegisterComponent("RigidBodyComponent", []() { return new ECS::RigidBodyComponent(); });
            RegisterComponent("BoxColliderComponent", []() { return new ECS::BoxColliderComponent(); });
            RegisterComponent("SphereColliderComponent", []() { return new ECS::SphereColliderComponent(); });
            RegisterComponent("CapsuleColliderComponent", []() { return new ECS::CapsuleColliderComponent(); });
            RegisterComponent("NavGridComponent", []() { return new ECS::NavGridComponent(); });
            RegisterComponent("NavAgentComponent", []() { return new ECS::NavAgentComponent(); });
            RegisterComponent("CameraComponent", []() { return new ECS::CameraComponent(); });
            RegisterComponent("FreeLookCamera", []() { return new ECS::FreeLookCamera(); });
            RegisterComponent("TrailRenderer", []() { return new ECS::TrailRenderer(); });
            RegisterComponent("ParticleSystem", []() { return new ECS::ParticleSystem(); });
            RegisterComponent("NetworkIdentity", []() { return new ECS::NetworkIdentity(); });
            RegisterComponent("NetworkTransform", []() { return new ECS::NetworkTransform(); });
            RegisterComponent("Animator", []() { return new Animation::Animator(); });
            RegisterComponent("Canvas", []() { return new UI::Canvas(); });
            RegisterComponent("UIText", []() { return new UI::UIText(); });
            RegisterComponent("UIImage", []() { return new UI::UIImage(); });
            RegisterComponent("UIPanel", []() { return new UI::UIPanel(); });
            RegisterComponent("UIButton", []() { return new UI::UIButton(); });
            RegisterComponent("UIInputField", []() { return new UI::UIInputField(); });
            RegisterComponent("UIContainer", []() { return new UI::UIContainer(); });
            RegisterComponent("UIHorizontalLayout", []() { return new UI::UIHorizontalLayout(); });
            RegisterComponent("UIVerticalLayout", []() { return new UI::UIVerticalLayout(); });
            RegisterComponent("UIJoystick", []() { return new UI::UIJoystick(); });
        }

    }
}
