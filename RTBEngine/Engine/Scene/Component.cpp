#include "Component.h"
#include "GameObject.h"
#include "Scene.h"
#include "../Core/Scheduler.h"
#include "../Core/TypeId.h"
#include "../Scripting/LatentActions.h"
#include "../Physics/CollisionInfo.h"

namespace RTBEngine {
    namespace Scene {

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

        void Component::InvalidateTickRegistration() const
        {
            if (!owner) {
                return;
            }

            if (Scene* scene = owner->GetOwningScene()) {
                scene->InvalidateTickCache();
            }
        }

        void Component::SetEnabled(bool enabled)
        {
            if (this->isEnabled == enabled) {
                return;
            }

            this->isEnabled = enabled;
            SyncEnabledState();
        }

        void Component::InvokeAwakeIfNeeded()
        {
            if (awakeInvoked) {
                return;
            }

            awakeInvoked = true;
            OnAwake();
        }

        void Component::ResetStartInvocation()
        {
            startInvoked = false;
            if (enabledInHierarchy && isEnabled) {
                owner->QueueComponentForStart(this);
            }
        }

        void Component::TryInvokeStart()
        {
            if (startInvoked) {
                return;
            }

            if (!isEnabled || !owner->IsActiveInHierarchy()) {
                return;
            }

            startInvoked = true;
            OnStart();
        }

        void Component::SyncEnabledState()
        {
            const bool shouldEnable = isEnabled && owner && owner->IsActiveInHierarchy();
            if (shouldEnable == enabledInHierarchy) {
                return;
            }

            if (shouldEnable) {
                enabledInHierarchy = true;
                InvalidateTickRegistration();
                OnEnable();
                if (isEnabled && owner->IsActiveInHierarchy()) {
                    owner->QueueComponentForStart(this);
                }
            } else {
                NotifyDisabled();
            }
        }

        void Component::NotifyDisabled()
        {
            if (!enabledInHierarchy) {
                return;
            }

            enabledInHierarchy = false;
            InvalidateTickRegistration();
            OnDisable();
        }

        std::size_t Component::FillTypeIds(std::uint32_t* out, std::size_t capacity) const
        {
            if (!out || capacity == 0) {
                return 0;
            }

            const char* typeName = GetTypeName();
            if (!typeName || typeName[0] == '\0') {
                return 0;
            }

            out[0] = TypeId::Hash(typeName);
            return 1;
        }

        void Component::SetUpdateTickEnabled(bool enabled)
        {
            if (this->updateTickEnabled == enabled) {
                return;
            }

            this->updateTickEnabled = enabled;
            InvalidateTickRegistration();
        }

        void Component::SetTimeMode(ComponentTimeMode mode)
        {
            this->timeMode = mode;
        }

        Scripting::LatentActionHandle Component::StartSequence(Scripting::LatentSequence sequence)
        {
            const bool useUnscaledTime = timeMode == ComponentTimeMode::Unscaled;
            return Core::Scheduler::GetInstance().StartSequence(
                this,
                std::move(sequence),
                useUnscaledTime);
        }

        Scripting::LatentActionHandle Component::Invoke(
            float delaySeconds,
            std::function<void()> callback)
        {
            const bool useUnscaledTime = timeMode == ComponentTimeMode::Unscaled;
            return Core::Scheduler::GetInstance().Invoke(
                this,
                delaySeconds,
                std::move(callback),
                useUnscaledTime);
        }

        Scripting::LatentActionHandle Component::InvokeRepeating(
            float initialDelaySeconds,
            float intervalSeconds,
            std::function<void()> callback)
        {
            const bool useUnscaledTime = timeMode == ComponentTimeMode::Unscaled;
            return Core::Scheduler::GetInstance().InvokeRepeating(
                this,
                initialDelaySeconds,
                intervalSeconds,
                std::move(callback),
                useUnscaledTime);
        }

        void Component::CancelInvoke(Scripting::LatentActionHandle handle)
        {
            Core::Scheduler::GetInstance().Cancel(handle);
        }

        void Component::CancelAllInvokes()
        {
            Core::Scheduler::GetInstance().CancelAllForOwner(this);
        }

    }
}
