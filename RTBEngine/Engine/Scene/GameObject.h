#pragma once

#include "../RTBEngineAPI.h"

#include "Transform.h"

#include "Component.h"
#include "StaticFlags.h"

#include "../Rendering/Camera.h"

#include "../Core/TypeId.h"

#include <string>

#include <vector>

#include <memory>

#include <functional>

#include <unordered_map>

#include <type_traits>

#include <cstdint>
#include <cstddef>



namespace RTBEngine {

    namespace Reflection {
        class TypeInfo;
    }

    namespace Scene {

        class Scene;

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

            void SetUUID(const std::string& id);

            static std::string GenerateNewUUID();



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

            bool IsActiveInHierarchy() const;

            bool IsBeingDestroyed() const { return isBeingDestroyed; }



            void SetTransient(bool t) { isTransient = t; }

            bool IsTransient() const { return isTransient; }



            void SetAnimatorBone(bool value) { isAnimatorBone = value; }

            bool IsAnimatorBone() const { return isAnimatorBone; }



            int GetCollisionLayer() const { return collisionLayer; }

            void SetCollisionLayer(int layerIndex);

            void SetCollisionLayerByName(const std::string& layerName);

            StaticFlags GetStaticFlags() const { return staticFlags; }

            void SetStaticFlags(StaticFlags flags) { staticFlags = flags; }

            bool HasStaticFlag(StaticFlags flag) const;

            void SetStaticFlag(StaticFlags flag, bool enabled);

            bool IsStatic() const;

            void SetStatic(bool enabled);



            Math::Matrix4 GetWorldMatrix() const;

            Math::Vector3 GetWorldPosition() const;

            Math::Quaternion GetWorldRotation() const;

            Math::Vector3 GetWorldScale() const;



            void Update(float deltaTime);

            void FixedUpdate(float fixedDeltaTime);

            void LateUpdate(float deltaTime);

            void Render(Rendering::Camera* camera);



            bool IsLifecycleInitialized() const { return lifecycleInitialized; }

            void SetLifecycleInitialized(bool initialized) { lifecycleInitialized = initialized; }

            // Fast-path lookup — implemented only in RTBEngine.dll (never touch private STL from GameScripts).
            Component* LookupComponentByTypeId(std::uint32_t typeId);
            Component* LookupComponentInChildrenByTypeId(std::uint32_t typeId, int maxDepth = -1);

            std::size_t GetComponentCount() const;
            Component* GetComponentAt(std::size_t index) const;

            std::size_t GetChildCount() const;
            GameObject* GetChildAt(std::size_t index) const;

            void SetOwningScene(Scene* scene) { owningScene = scene; }
            Scene* GetOwningScene() const { return owningScene; }

        private:

            friend class Transform;

            void RegisterComponentType(Component* component);

            void UnregisterComponentType(Component* component);



            void MarkWorldMatrixDirty();
            void MarkActiveInHierarchyDirty();

            std::string name;

            std::string uuid;

            std::string prefabName;



            Transform transform;

            std::vector<ComponentPtr> components;

            std::unordered_map<std::uint32_t, Component*> componentsByTypeId;

            bool isActive;

            bool lifecycleInitialized = false;

            bool isBeingDestroyed = false;

            bool isTransient = false;

            bool isAnimatorBone = false;

            int collisionLayer = 0;

            StaticFlags staticFlags = StaticFlags::None;



            GameObject* parent = nullptr;

            std::vector<GameObject*> children;

            Scene* owningScene = nullptr;

            mutable Math::Matrix4 cachedWorldMatrix;
            mutable bool worldMatrixDirty = true;

            mutable bool cachedActiveInHierarchy = true;
            mutable bool activeInHierarchyDirty = true;

        };

        #pragma warning(pop)



        namespace ComponentLookup {



            template<typename T, typename = void>

            struct HasTypeId : std::false_type {};



            template<typename T>

            struct HasTypeId<T, std::void_t<decltype(T::TypeId())>> : std::true_type {};



        } // namespace ComponentLookup



        template<typename T>

        T* GameObject::GetComponent()

        {

            if constexpr (ComponentLookup::HasTypeId<T>::value) {

                constexpr std::uint32_t typeId = T::TypeId();

                if (typeId != 0) {

                    if (Component* cached = LookupComponentByTypeId(typeId)) {

                        if (T* result = dynamic_cast<T*>(cached)) {

                            return result;

                        }

                    }

                }

            }



            // Slow path: abstract bases (UIElement, CharacterBase) and types without RTB_COMPONENT.
            for (std::size_t i = 0, count = GetComponentCount(); i < count; ++i) {

                if (T* castedComp = dynamic_cast<T*>(GetComponentAt(i))) {

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

            if constexpr (ComponentLookup::HasTypeId<T>::value) {

                constexpr std::uint32_t typeId = T::TypeId();

                if (typeId != 0) {

                    if (Component* cached = LookupComponentInChildrenByTypeId(typeId, maxDepth)) {

                        if (T* result = dynamic_cast<T*>(cached)) {

                            return result;

                        }

                    }

                }

            }



            if (T* result = GetComponent<T>()) {

                return result;

            }



            if (maxDepth == 0) return nullptr;



            const int childDepth = (maxDepth > 0) ? maxDepth - 1 : -1;

            for (std::size_t i = 0, count = GetChildCount(); i < count; ++i) {

                GameObject* child = GetChildAt(i);

                if (!child) continue;

                if (T* result = child->GetComponentInChildren<T>(childDepth)) {

                    return result;

                }

            }

            return nullptr;

        }



    }

}


