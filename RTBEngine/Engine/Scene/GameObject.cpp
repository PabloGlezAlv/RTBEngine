#include "GameObject.h"
#include "Scene.h"
#include "SceneLifecycle.h"
#include "../Core/Scheduler.h"
#include "../Core/Time.h"
#include "../Reflection/TypeInfo.h"
#include "../Core/Logger.h"
#include "../Physics/PhysicsLayerSettings.h"
#include <algorithm>
#include <objbase.h>
#include <cstdio>


namespace RTBEngine {
    namespace Scene {
        static std::string GenerateUUID()
        {
            GUID guid;
            CoCreateGuid(&guid);
            char buf[37];
            snprintf(buf, sizeof(buf),
                "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                guid.Data1, guid.Data2, guid.Data3,
                guid.Data4[0], guid.Data4[1],
                guid.Data4[2], guid.Data4[3], guid.Data4[4],
                guid.Data4[5], guid.Data4[6], guid.Data4[7]);
            return std::string(buf);
        }

        std::string GameObject::GenerateNewUUID()
        {
            return GenerateUUID();
        }

        void GameObject::SetUUID(const std::string& id)
        {
            if (uuid == id) {
                return;
            }

            Scene* scene = owningScene;
            if (scene && !uuid.empty()) {
                scene->UnregisterGameObjectUuid(this);
            }

            uuid = id;

            if (scene && !uuid.empty()) {
                scene->RegisterGameObjectUuid(this);
            }
        }


        GameObject::GameObject(const std::string& name)
            : name(name)
            , uuid(GenerateUUID())
            , transform()
            , isActive(true)
        {
            transform.SetOwningGameObject(this);
        }

        // Returns a deleter that destroys the component through its TypeInfo if available.
        // This ensures delete runs in the same module (heap) that called new,
        // which is required when GameScripts.dll uses a separate /MT CRT heap.
        static std::function<void(Component*)> MakeComponentDeleter(
            Component* component,
            const RTBEngine::Reflection::TypeInfo* typeInfoOverride)
        {
            if (!component) return [](Component* c) { delete c; };

            const RTBEngine::Reflection::TypeInfo* ti = typeInfoOverride;
            if (!ti) {
                ti = component->GetTypeInfo();
            }

            if (ti) {
                return [ti](Component* c) { ti->Destroy(c); };
            }
            return [](Component* c) { delete c; };
        }

        static uint32_t s_globalHierarchyVersion = 0;

        uint32_t GameObject::GetHierarchyVersion() { return s_globalHierarchyVersion; }

        GameObject::~GameObject()
        {
            isBeingDestroyed = true;

            for (GameObject* child : children) {
                if (child) {
                    child->parent = nullptr;
                }
            }
            children.clear();

            if (parent && !parent->IsBeingDestroyed()) {
                parent->RemoveChild(this);
            }
            parent = nullptr;

            for (auto& comp : components) {
                Core::Scheduler::GetInstance().CancelAllForOwner(comp.get());
                comp->NotifyDisabled();
                comp->OnDestroy();
            }
            components.clear();
            componentsByTypeId.clear();
        }

        void GameObject::AddComponent(Component* component)
        {
            AddComponent(component, nullptr);
        }

        Component* GameObject::LookupComponentByTypeId(const std::uint32_t typeId)
        {
            if (typeId == 0) {
                return nullptr;
            }

            const auto it = componentsByTypeId.find(typeId);
            return it != componentsByTypeId.end() ? it->second : nullptr;
        }

        Component* GameObject::LookupComponentInChildrenByTypeId(const std::uint32_t typeId, int maxDepth)
        {
            if (typeId == 0) {
                return nullptr;
            }

            if (Component* local = LookupComponentByTypeId(typeId)) {
                return local;
            }

            if (maxDepth == 0) {
                return nullptr;
            }

            const int childDepth = (maxDepth > 0) ? maxDepth - 1 : -1;
            for (GameObject* child : children) {
                if (!child) {
                    continue;
                }

                if (Component* found = child->LookupComponentInChildrenByTypeId(typeId, childDepth)) {
                    return found;
                }
            }

            return nullptr;
        }

        std::size_t GameObject::GetComponentCount() const
        {
            return components.size();
        }

        Component* GameObject::GetComponentAt(const std::size_t index) const
        {
            return index < components.size() ? components[index].get() : nullptr;
        }

        std::size_t GameObject::GetChildCount() const
        {
            return children.size();
        }

        GameObject* GameObject::GetChildAt(const std::size_t index) const
        {
            return index < children.size() ? children[index] : nullptr;
        }

        constexpr std::size_t kMaxComponentTypeIds = 8;

        static std::size_t CopyComponentTypeIds(const Component* component, std::uint32_t* out)
        {
            if (!component || !out) {
                return 0;
            }

            return component->FillTypeIds(out, kMaxComponentTypeIds);
        }

        static bool ComponentHasTypeId(const Component* component, const std::uint32_t typeId)
        {
            if (!component || typeId == 0) {
                return false;
            }

            std::uint32_t ids[kMaxComponentTypeIds];
            const std::size_t count = CopyComponentTypeIds(component, ids);
            for (std::size_t i = 0; i < count; ++i) {
                if (ids[i] == typeId) {
                    return true;
                }
            }

            return false;
        }

        void GameObject::RegisterComponentType(Component* component)
        {
            if (!component) {
                return;
            }

            std::uint32_t ids[kMaxComponentTypeIds];
            const std::size_t count = CopyComponentTypeIds(component, ids);
            for (std::size_t i = 0; i < count; ++i) {
                if (ids[i] != 0) {
                    componentsByTypeId.emplace(ids[i], component);
                }
            }
        }

        void GameObject::UnregisterComponentType(Component* component)
        {
            if (!component) {
                return;
            }

            std::uint32_t ids[kMaxComponentTypeIds];
            const std::size_t count = CopyComponentTypeIds(component, ids);
            for (std::size_t i = 0; i < count; ++i) {
                const std::uint32_t typeId = ids[i];
                if (typeId == 0) {
                    continue;
                }

                const auto cached = componentsByTypeId.find(typeId);
                if (cached == componentsByTypeId.end() || cached->second != component) {
                    continue;
                }

                componentsByTypeId.erase(cached);

                for (const auto& comp : components) {
                    if (!comp || comp.get() == component) {
                        continue;
                    }

                    if (ComponentHasTypeId(comp.get(), typeId)) {
                        componentsByTypeId.emplace(typeId, comp.get());
                        break;
                    }
                }
            }
        }

        void GameObject::AddComponent(Component* component, const RTBEngine::Reflection::TypeInfo* typeInfoOverride)
        {
            if (!component) {
                return;
            }

            component->SetOwner(this);
            auto deleter = MakeComponentDeleter(component, typeInfoOverride);
            components.push_back(std::unique_ptr<Component, std::function<void(Component*)>>(component, std::move(deleter)));
            RegisterComponentType(component);

            if (owningScene) {
                owningScene->InvalidateComponentCaches();
            }

            if (lifecycleInitialized) {
                SceneLifecycle::InvokeAwakeAndValidate(component);
                component->SyncEnabledState();
            }
        }

        void GameObject::RemoveComponent(Component* component)
        {
            auto it = std::find_if(components.begin(), components.end(),
                [component]( const std::unique_ptr<Component, std::function<void(Component*)>>& comp) {
                    return comp.get() == component;
                });

            if (it != components.end()) {
                UnregisterComponentType(component);
                Core::Scheduler::GetInstance().CancelAllForOwner(component);
                (*it)->NotifyDisabled();
                (*it)->OnDestroy();
                components.erase(it);

                if (owningScene) {
                    owningScene->InvalidateComponentCaches();
                }
            }
        }

        void GameObject::SetActive(bool active)
        {
            if (isActive == active) {
                return;
            }

            isActive = active;
            MarkActiveInHierarchyDirty();

            if (lifecycleInitialized) {
                SyncHierarchyEnabledState();
            }
        }

        void GameObject::SyncEnabledState()
        {
            for (auto& component : components) {
                if (component) {
                    component->SyncEnabledState();
                }
            }
        }

        void GameObject::SyncHierarchyEnabledState()
        {
            SyncEnabledState();
            for (GameObject* child : children) {
                if (child) {
                    child->SyncHierarchyEnabledState();
                }
            }
        }

        bool GameObject::IsActiveInHierarchy() const
        {
            if (!activeInHierarchyDirty) {
                return cachedActiveInHierarchy;
            }

            if (parent) {
                cachedActiveInHierarchy = isActive && parent->IsActiveInHierarchy();
            } else {
                cachedActiveInHierarchy = isActive;
            }

            activeInHierarchyDirty = false;
            return cachedActiveInHierarchy;
        }

        void GameObject::MarkActiveInHierarchyDirty()
        {
            activeInHierarchyDirty = true;
            for (GameObject* child : children) {
                if (child) {
                    child->MarkActiveInHierarchyDirty();
                }
            }
        }

        void GameObject::MarkWorldMatrixDirty()
        {
            if (worldMatrixDirty) {
                return;
            }

            worldMatrixDirty = true;
            for (GameObject* child : children) {
                if (child) {
                    child->MarkWorldMatrixDirty();
                }
            }
        }

        Math::Matrix4 GameObject::GetWorldMatrix() const
        {
            if (!worldMatrixDirty) {
                return cachedWorldMatrix;
            }

            if (parent) {
                cachedWorldMatrix = parent->GetWorldMatrix() * transform.GetModelMatrix();
            } else {
                cachedWorldMatrix = transform.GetModelMatrix();
            }

            worldMatrixDirty = false;
            return cachedWorldMatrix;
        }

        Math::Vector3 GameObject::GetWorldPosition() const
        {
            if (parent) {
                const Math::Matrix4& worldMatrix = GetWorldMatrix();
                return Math::Vector3(worldMatrix[12], worldMatrix[13], worldMatrix[14]);
            }
            return transform.GetPosition();
        }

        Math::Quaternion GameObject::GetWorldRotation() const
        {
            if (parent) {
                return parent->GetWorldRotation() * transform.GetRotation();
            }
            return transform.GetRotation();
        }

        Math::Vector3 GameObject::GetWorldScale() const
        {
            if (parent) {
                Math::Vector3 parentScale = parent->GetWorldScale();
                Math::Vector3 localScale = transform.GetScale();
                return Math::Vector3(parentScale.x * localScale.x, parentScale.y * localScale.y, parentScale.z * localScale.z);
            }
            return transform.GetScale();
        }

        void GameObject::Update(float deltaTime)
        {
            if (!IsActiveInHierarchy()) return;
            (void)deltaTime;

            for (auto& component : components) {
                if (component) {
                    component->TryInvokeStart();
                }
            }

            for (auto& comp : components) {
                if (comp->IsEnabled() && comp->IsUpdateTickEnabled()) {
                    if (comp->GetTimeMode() == ComponentTimeMode::Unscaled) {
                        comp->OnUpdate(Core::Time::GetUnscaledDeltaTime());
                    } else if (!Core::Time::IsPaused()) {
                        comp->OnUpdate(Core::Time::GetDeltaTime());
                    }
                }
            }
        }

        void GameObject::FixedUpdate(float fixedDeltaTime)
        {
            if (!IsActiveInHierarchy()) return;

            for (auto& comp : components) {
                if (comp->IsEnabled()) {
                    comp->OnFixedUpdate(fixedDeltaTime);
                }
            }
        }

        void GameObject::LateUpdate(float deltaTime)
        {
            if (!IsActiveInHierarchy()) return;
            (void)deltaTime;

            for (auto& comp : components) {
                if (comp->IsEnabled() && comp->IsUpdateTickEnabled()) {
                    if (comp->GetTimeMode() == ComponentTimeMode::Unscaled) {
                        comp->OnLateUpdate(Core::Time::GetUnscaledDeltaTime());
                    } else if (!Core::Time::IsPaused()) {
                        comp->OnLateUpdate(Core::Time::GetDeltaTime());
                    }
                }
            }
        }

        void GameObject::Render(Rendering::Camera* camera)
        {
            
        }

        void GameObject::SetParent(GameObject* newParent)
        {
            if (newParent == parent) return;

            GameObject* oldParent = parent;

            if (parent) {
                parent->RemoveChild(this);
            }

            parent = newParent;

            if (parent) {
                parent->AddChild(this);
            }

            for (auto& comp : components) {
                Component* component = comp.get();
                if (component) {
                    component->OnParentChanged(oldParent, parent);
                }
            }

            MarkWorldMatrixDirty();
            MarkActiveInHierarchyDirty();

            if (lifecycleInitialized) {
                SyncHierarchyEnabledState();
            }
        }

        void GameObject::AddChild(GameObject* child)
        {
            if (!child) return;

            auto it = std::find(children.begin(), children.end(), child);
            if (it == children.end()) {
                children.push_back(child);
                ++s_globalHierarchyVersion;
            }
        }

        void GameObject::RemoveChild(GameObject* child)
        {
            if (!child) return;

            auto it = std::find(children.begin(), children.end(), child);
            if (it != children.end()) {
                children.erase(it);
                ++s_globalHierarchyVersion;
            }
        }

        void GameObject::SetCollisionLayer(int layerIndex)
        {
            collisionLayer = std::clamp(layerIndex, 0, Physics::kMaxPhysicsLayers - 1);
        }

        void GameObject::SetCollisionLayerByName(const std::string& layerName)
        {
            SetCollisionLayer(Physics::PhysicsLayerSettings::Get().GetLayerIndex(layerName));
        }

        void GameObject::SetStaticFlag(StaticFlags flag, bool enabled)
        {
            if (enabled) {
                staticFlags |= flag;
            } else {
                staticFlags &= ~flag;
            }
        }

        bool GameObject::HasStaticFlag(StaticFlags flag) const
        {
            if ((staticFlags & flag) != StaticFlags::None) {
                return true;
            }

            return parent && parent->HasStaticFlag(flag);
        }

        bool GameObject::IsStatic() const
        {
            if (staticFlags != StaticFlags::None) {
                return true;
            }

            return parent && parent->IsStatic();
        }

        void GameObject::SetStatic(bool enabled)
        {
            // Do not include Occluder by default: that flag used to drive camera-fade on every
            // static mesh and made locomotion look broken. Use Occludable for fade instead.
            constexpr StaticFlags kDefaultStaticFlags =
                StaticFlags::Batching | StaticFlags::ContributeGI | StaticFlags::Navigation;
            staticFlags = enabled ? kDefaultStaticFlags : StaticFlags::None;
        }

    }
}
