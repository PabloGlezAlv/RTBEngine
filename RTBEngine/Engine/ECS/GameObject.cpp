#include "GameObject.h"
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

        GameObject::~GameObject()
        {
            for (auto& comp : components) {
                comp->OnDestroy();
            }
            components.clear();
        }

        void GameObject::AddComponent(Component* component)
        {
            if (component) {
                component->SetOwner(this);
                components.push_back(std::unique_ptr<Component>(component));
                component->OnAwake();
            }
        }

        void GameObject::RemoveComponent(Component* component)
        {
            auto it = std::find_if(components.begin(), components.end(),
                [component](const std::unique_ptr<Component>& comp) {
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

            if (!started) {
                for (auto& comp : components) {
                    if (comp->IsEnabled()) {
                        comp->OnStart();
                    }
                }
                started = true;
            }

            for (auto& comp : components) {
                if (comp->IsEnabled()) {
                    comp->OnUpdate(deltaTime);
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

        void GameObject::Render(Rendering::Camera* camera)
        {
            
        }

        void GameObject::SetParent(GameObject* newParent)
        {
            if (parent) {
                parent->RemoveChild(this);
            }

            parent = newParent;

            if (parent) {
                parent->AddChild(this);
            }
        }

        void GameObject::AddChild(GameObject* child)
        {
            if (!child) return;

            auto it = std::find(children.begin(), children.end(), child);
            if (it == children.end()) {
                children.push_back(child);
            }
        }

        void GameObject::RemoveChild(GameObject* child)
        {
            if (!child) return;

            auto it = std::find(children.begin(), children.end(), child);
            if (it != children.end()) {
                children.erase(it);
            }
        }

    }
}