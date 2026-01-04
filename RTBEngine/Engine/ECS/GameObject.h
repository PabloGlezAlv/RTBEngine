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

            const std::vector<std::unique_ptr<Component>>& GetComponents() const { return components; }

            Transform& GetTransform() { return transform; }
            const Transform& GetTransform() const { return transform; }
            const std::string& GetName() const { return name; }
            void SetName(const std::string& name) { this->name = name; }

            void SetParent(GameObject* newParent);
            GameObject* GetParent() const { return parent; }
            void AddChild(GameObject* child);
            void RemoveChild(GameObject* child);
            const std::vector<GameObject*>& GetChildren() const { return children; }


            void SetActive(bool active);
            bool IsActive() const { return isActive; }

            Math::Matrix4 GetWorldMatrix() const;
            Math::Vector3 GetWorldPosition() const;
            Math::Quaternion GetWorldRotation() const;
            Math::Vector3 GetWorldScale() const;

            void Update(float deltaTime);
            void FixedUpdate(float fixedDeltaTime);
            void Render(Rendering::Camera* camera);

        private:
            std::string name;
            Transform transform;
            std::vector<std::unique_ptr<Component>> components;
            bool isActive;
            bool started;

            GameObject* parent = nullptr;
            std::vector<GameObject*> children;
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