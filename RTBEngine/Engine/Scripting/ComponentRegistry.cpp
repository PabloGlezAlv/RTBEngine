#include "ComponentRegistry.h"
#include <iostream>
#include "../RTBEngine.h"
#include "../Data/DataAssetRegistry.h"
#include "../Reflection/TypeInfo.h"

#include "../Scene/NetworkIdentity.h"
#include "../Scene/NetworkTransform.h"
#include "../Online/OnlineGameplayNet.h"
#include "../Scene/MeshRenderer.h"
#include "../Scene/LightComponent.h"
#include "../Scene/VolumeComponent.h"
#include "../Scene/AudioSourceComponent.h"
#include "../Scene/RigidBodyComponent.h"
#include "../Scene/BoxColliderComponent.h"
#include "../Scene/SphereColliderComponent.h"
#include "../Scene/CapsuleColliderComponent.h"
#include "../Scene/NavGridComponent.h"
#include "../Scene/NavAgentComponent.h"
#include "../Scene/CameraComponent.h"
#include "../Scene/FreeLookCamera.h"
#include "../Scene/TrailRenderer.h"
#include "../Scene/ParticleSystem.h"
#include "../Scene/AnimatedBillboard.h"
#include "../Scene/Occludable.h"
#include "../Scene/OcclusionTarget.h"
#include "../Scene/OcclusionFadeController.h"
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
            std::function<Scene::Component* ()> factory) {
            factories[typeName] = factory;
        }

        Scene::Component* ComponentRegistry::CreateComponent(const std::string& typeName) {
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

        void ComponentRegistry::DestroyComponent(const std::string& typeName, Scene::Component* component) const {
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
            if (RTBEngine::Data::DataAssetRegistry::GetInstance().IsDataAssetType(typeName)) {
                return false;
            }

            if (factories.find(typeName) != factories.end()) return true;

            const Reflection::TypeInfo* typeInfo =
                Reflection::TypeRegistry::GetInstance().GetTypeInfo(typeName);
            if (typeInfo && typeInfo->IsDataAsset()) {
                return false;
            }

            return Reflection::TypeRegistry::GetInstance().HasType(typeName);
        }

        void ComponentRegistry::RegisterBuiltInComponents() {
            RegisterComponent("MeshRenderer", []() { return new Scene::MeshRenderer(); });
            RegisterComponent("LightComponent", []() { return new Scene::LightComponent(); });
            RegisterComponent("VolumeComponent", []() { return new Scene::VolumeComponent(); });
            RegisterComponent("AudioSourceComponent", []() { return new Scene::AudioSourceComponent(); });
            RegisterComponent("RigidBodyComponent", []() { return new Scene::RigidBodyComponent(); });
            RegisterComponent("BoxColliderComponent", []() { return new Scene::BoxColliderComponent(); });
            RegisterComponent("SphereColliderComponent", []() { return new Scene::SphereColliderComponent(); });
            RegisterComponent("CapsuleColliderComponent", []() { return new Scene::CapsuleColliderComponent(); });
            RegisterComponent("NavGridComponent", []() { return new Scene::NavGridComponent(); });
            RegisterComponent("NavAgentComponent", []() { return new Scene::NavAgentComponent(); });
            RegisterComponent("CameraComponent", []() { return new Scene::CameraComponent(); });
            RegisterComponent("FreeLookCamera", []() { return new Scene::FreeLookCamera(); });
            RegisterComponent("TrailRenderer", []() { return new Scene::TrailRenderer(); });
            RegisterComponent("ParticleSystem", []() { return new Scene::ParticleSystem(); });
            RegisterComponent("AnimatedBillboard", []() { return new Scene::AnimatedBillboard(); });
            RegisterComponent("NetworkIdentity", []() { return new Scene::NetworkIdentity(); });
            RegisterComponent("NetworkTransform", []() { return new Scene::NetworkTransform(); });
            RegisterComponent("Occludable", []() { return new Scene::Occludable(); });
            RegisterComponent("OcclusionTarget", []() { return new Scene::OcclusionTarget(); });
            RegisterComponent("OcclusionFadeController", []() { return new Scene::OcclusionFadeController(); });
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
