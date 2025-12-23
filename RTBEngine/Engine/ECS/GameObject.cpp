#include "GameObject.h"

namespace RTBEngine {
    namespace ECS {

        GameObject::GameObject(const std::string& name)
            : name(name)
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