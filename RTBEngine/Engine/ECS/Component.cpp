#include "Component.h"
#include "../Physics/CollisionInfo.h"

namespace RTBEngine {
    namespace ECS {

        Component::Component()
            : owner(nullptr)
            , isEnabled(true)
            , updateTickEnabled(true)
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

        void Component::SetUpdateTickEnabled(bool enabled)
        {
            this->updateTickEnabled = enabled;
        }

    }
}
