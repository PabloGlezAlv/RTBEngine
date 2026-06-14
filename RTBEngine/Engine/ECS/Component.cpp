#include "Component.h"
#include "GameObject.h"
#include "../Physics/CollisionInfo.h"

namespace RTBEngine {
    namespace ECS {

        Component::Component()
            : owner(nullptr)
            , isEnabled(true)
            , updateTickEnabled(true)
            , timeMode(ComponentTimeMode::Scaled)
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
            const bool wasEnabled = this->isEnabled;
            this->isEnabled = enabled;
            if (!wasEnabled && enabled) {
                TryInvokeStart();
            }
        }

        void Component::InvokeAwakeIfNeeded()
        {
            if (awakeInvoked) {
                return;
            }

            awakeInvoked = true;
            OnAwake();
        }

        void Component::TryInvokeStart()
        {
            if (startInvoked || !owner || !isEnabled || !owner->IsActiveInHierarchy()) {
                return;
            }

            startInvoked = true;
            OnStart();
        }

        void Component::SetUpdateTickEnabled(bool enabled)
        {
            this->updateTickEnabled = enabled;
        }

        void Component::SetTimeMode(ComponentTimeMode mode)
        {
            this->timeMode = mode;
        }

    }
}
