#include "GameObject.h"
#include "../Core/Time.h"
#include "../Reflection/TypeInfo.h"
#include "../Core/Logger.h"
#include <objbase.h>
#include <cstdio>


namespace RTBEngine {
    namespace ECS {
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


        GameObject::GameObject(const std::string& name)
            : name(name)
            , uuid(GenerateUUID())
            , transform()
            , isActive(true)
            , started(false)
        {
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

            for (auto& comp : components) {
                comp->OnDestroy();
            }
            components.clear();
        }

        void GameObject::AddComponent(Component* component)
        {
            AddComponent(component, nullptr);
        }

        void GameObject::AddComponent(Component* component, const RTBEngine::Reflection::TypeInfo* typeInfoOverride)
        {
            if (component) {
                component->SetOwner(this);
                auto deleter = MakeComponentDeleter(component, typeInfoOverride);
                components.push_back(std::unique_ptr<Component, std::function<void(Component*)>>(component, std::move(deleter)));
                component->OnAwake();
            }
        }

        void GameObject::RemoveComponent(Component* component)
        {
            auto it = std::find_if(components.begin(), components.end(),
                [component](const std::unique_ptr<Component, std::function<void(Component*)>>& comp) {
                    return comp.get() == component;
                });

            if (it != components.end()) {
                (*it)->OnDestroy();
                components.erase(it);
            }
        }

        void GameObject::SetActive(bool active)
        {
            this->isActive = active;
        }

        Math::Matrix4 GameObject::GetWorldMatrix() const
        {
            if (parent) {
                return parent->GetWorldMatrix() * transform.GetModelMatrix();
            }
            return transform.GetModelMatrix();
        }

        Math::Vector3 GameObject::GetWorldPosition() const
        {
            if (parent) {
                // Return translation part of world matrix
                Math::Matrix4 wm = GetWorldMatrix();
                return Math::Vector3(wm[12], wm[13], wm[14]);
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
            if (!isActive) return;
            (void)deltaTime;

            if (!started) {
                for (size_t i = 0; i < components.size(); ++i) {
                    Component* component = components[i].get();

                    if (!component) {
                        RTB_WARN("GameObject::Update encountered null component during OnStart.");
                        continue;
                    }

                    if (component->IsEnabled()) {
                        component->OnStart();
                    }
                }
                started = true;
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
            if (!isActive) return;

            for (auto& comp : components) {
                if (comp->IsEnabled()) {
                    comp->OnFixedUpdate(fixedDeltaTime);
                }
            }
        }

        void GameObject::LateUpdate(float deltaTime)
        {
            if (!isActive) return;
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

    }
}
