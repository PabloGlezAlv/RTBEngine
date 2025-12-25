#include "ComponentRegistry.h"
#include <iostream>

#include "../ECS/MeshRenderer.h"
#include "../ECS/LightComponent.h"
#include "../ECS/AudioSourceComponent.h"
#include "../ECS/RigidBodyComponent.h"
#include "../ECS/CameraComponent.h"
#include "../UI/Canvas.h"
#include "../UI/Elements/UIText.h"
#include "../UI/Elements/UIImage.h"
#include "../UI/Elements/UIPanel.h"
#include "../UI/Elements/UIButton.h"

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

            std::cerr << "ComponentRegistry: Component type '" << typeName << "' not registered!" << std::endl;
            return nullptr;
        }

        bool ComponentRegistry::HasComponent(const std::string& typeName) const {
            return factories.find(typeName) != factories.end();
        }

        void ComponentRegistry::RegisterBuiltInComponents() {
            RegisterComponent("MeshRenderer", []() { return new ECS::MeshRenderer(); });
            RegisterComponent("LightComponent", []() { return new ECS::LightComponent(); });
            RegisterComponent("AudioSourceComponent", []() { return new ECS::AudioSourceComponent(); });
            RegisterComponent("RigidBodyComponent", []() { return new ECS::RigidBodyComponent(); });
            RegisterComponent("CameraComponent", []() { return new ECS::CameraComponent(); });
            RegisterComponent("Canvas", []() { return new UI::Canvas(); });
            RegisterComponent("UIText", []() { return new UI::UIText(); });
            RegisterComponent("UIImage", []() { return new UI::UIImage(); });
            RegisterComponent("UIPanel", []() { return new UI::UIPanel(); });
            RegisterComponent("UIButton", []() { return new UI::UIButton(); });
        }

    }
}
