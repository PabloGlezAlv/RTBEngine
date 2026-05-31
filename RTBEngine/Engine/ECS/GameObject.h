#pragma once
#include "../RTBEngineAPI.h"
#include "Transform.h"
#include "Component.h"
#include "../Rendering/Camera.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <typeinfo>

namespace RTBEngine {
    namespace Reflection {
        class TypeInfo;
    }

    namespace ECS {

        #pragma warning(push)
        #pragma warning(disable: 4251)
        class RTB_API GameObject {
        public:
            GameObject(const std::string& name = "GameObject");
            ~GameObject();

            GameObject(const GameObject&) = delete;
            GameObject& operator=(const GameObject&) = delete;

            void AddComponent(Component* component);
            void AddComponent(Component* component, const Reflection::TypeInfo* typeInfoOverride);
            void RemoveComponent(Component* component);

            template<typename T>
            T* GetComponent();

            template<typename T>
            bool HasComponent();

            template<typename T>
            T* GetComponentInChildren(int maxDepth = -1);

            using ComponentPtr = std::unique_ptr<Component, std::function<void(Component*)>>;
            const std::vector<ComponentPtr>& GetComponents() const { return components; }

            Transform& GetTransform() { return transform; }
            const Transform& GetTransform() const { return transform; }
            const std::string& GetName() const { return name; }
            // ABI-safe accessor for use from script DLLs with separate CRT heaps.
            const char* GetNameCStr() const { return name.c_str(); }
            void SetName(const std::string& name) { this->name = name; }

            const std::string& GetUUID() const { return uuid; }
            void SetUUID(const std::string& id) { uuid = id; }

            const std::string& GetPrefabName() const { return prefabName; }
            void SetPrefabName(const std::string& name) { prefabName = name; }
            bool IsPrefabInstance() const { return !prefabName.empty(); }


            void SetParent(GameObject* newParent);
            GameObject* GetParent() const { return parent; }
            void AddChild(GameObject* child);
            void RemoveChild(GameObject* child);
            const std::vector<GameObject*>& GetChildren() const { return children; }

            // Incremented on every AddChild/RemoveChild.
            static uint32_t GetHierarchyVersion();

            void SetActive(bool active);
            bool IsActive() const { return isActive; }
            bool IsBeingDestroyed() const { return isBeingDestroyed; }

            void SetTransient(bool t) { isTransient = t; }
            bool IsTransient() const { return isTransient; }

            int GetCollisionLayer() const { return collisionLayer; }
            void SetCollisionLayer(int layerIndex);
            void SetCollisionLayerByName(const std::string& layerName);

            Math::Matrix4 GetWorldMatrix() const;
            Math::Vector3 GetWorldPosition() const;
            Math::Quaternion GetWorldRotation() const;
            Math::Vector3 GetWorldScale() const;

            void Update(float deltaTime);
            void FixedUpdate(float fixedDeltaTime);
            void LateUpdate(float deltaTime);
            void Render(Rendering::Camera* camera);

        private:
            std::string name;
            std::string uuid;
            std::string prefabName;

            Transform transform;
            std::vector<ComponentPtr> components;
            bool isActive;
            bool started;
            bool isBeingDestroyed = false;
            bool isTransient = false;
            int collisionLayer = 0;

            GameObject* parent = nullptr;
            std::vector<GameObject*> children;
        };
        #pragma warning(pop)

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

        template<typename T>
        T* GameObject::GetComponentInChildren(int maxDepth)
        {
            T* result = GetComponent<T>();
            if (result) return result;

            if (maxDepth == 0) return nullptr;

            const int childDepth = (maxDepth > 0) ? maxDepth - 1 : -1;
            for (GameObject* child : children) {
                if (!child) continue;
                result = child->GetComponentInChildren<T>(childDepth);
                if (result) return result;
            }
            return nullptr;
        }

    }
}
