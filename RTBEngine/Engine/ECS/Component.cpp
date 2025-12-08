#include "Component.h"

namespace RTBEngine {
    namespace ECS {

        Component::Component()
            : owner(nullptr)
            , isEnabled(true)
        {
        }

        Component::~Component()
        {
        }

        void Component::SetOwner(GameObject* owner)
        {
            this->owner = owner;
        }

        void Component::SetEnabled(bool enabled)
        {
            this->isEnabled = enabled;
        }

    }
}