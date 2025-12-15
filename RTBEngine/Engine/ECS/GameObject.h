#pragma once
#include "Transform.h"
#include "Component.h"
#include "../Rendering/Camera.h"
#include <string>
#include <vector>
#include <memory>
#include <typeinfo>

namespace RTBEngine {
    namespace ECS {

        class GameObject {
        public:
            GameObject(const std::string& name = "GameObject");
            ~GameObject();

            GameObject(const GameObject&) = delete;
            GameObject& operator=(const GameObject&) = delete;

            void AddComponent(Component* component);
            void RemoveComponent(Component* component);

            template<typename T>
            T* GetComponent();

            template<typename T>
            bool HasComponent();

            Transform& GetTransform() { return transform; }
            const Transform& GetTransform() const { return transform; }
            const std::string& GetName() const { return name; }

            void SetActive(bool active);
            bool IsActive() const { return isActive; }

            void Update(float deltaTime);
            void Render(Rendering::Camera* camera);

        private:
            std::string name;
            Transform transform;
            std::vector<std::unique_ptr<Component>> components;
            bool isActive;
            bool started;
        };

        template<typename T>
        T* GameObject::GetComponent()
        {
            for (auto& comp : components) {
                T* castedComp = dynamic_cast<T*>(comp.get());
                if (castedComp != nullptr) {
                    return castedComp;
                }
            }
            return nullptr;
        }

        template<typename T>
        bool GameObject::HasComponent()
        {
            return GetComponent<T>() != nullptr;
        }

    }
}