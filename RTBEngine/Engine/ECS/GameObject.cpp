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
            for (Component* comp : components) {
                comp->OnDestroy();
                delete comp;
            }
            components.clear();
        }

        void GameObject::AddComponent(Component* component)
        {
            if (component) {
                component->SetOwner(this);
                components.push_back(component);
                component->OnAwake();
            }
        }

        void GameObject::RemoveComponent(Component* component)
        {
            auto it = std::find(components.begin(), components.end(), component);
            if (it != components.end()) {
                (*it)->OnDestroy();
                delete* it;
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
                for (Component* comp : components) {
                    if (comp->IsEnabled()) {
                        comp->OnStart();
                    }
                }
                started = true;
            }

            for (Component* comp : components) {
                if (comp->IsEnabled()) {
                    comp->OnUpdate(deltaTime);
                }
            }
        }

        void GameObject::Render(Rendering::Camera* camera)
        {
            
        }

    }
}