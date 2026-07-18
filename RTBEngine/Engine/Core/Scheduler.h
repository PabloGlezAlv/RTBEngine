#pragma once

#include "../RTBEngineAPI.h"
#include "../Scripting/LatentActions.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace RTBEngine {
    namespace Scene {
        class Component;
    }

    namespace Core {

        // Central latent-action scheduler (Unity-style Invoke + sequences). Not a Component.
        // Ticks from Application; actions can be bound to a Component for auto-cancel on destroy.
        class RTB_API Scheduler {
        public:
            static Scheduler& GetInstance();

            Scripting::LatentActionHandle StartSequence(
                Scene::Component* owner,
                Scripting::LatentSequence sequence,
                bool useUnscaledTime);

            Scripting::LatentActionHandle Invoke(
                Scene::Component* owner,
                float delaySeconds,
                std::function<void()> callback,
                bool useUnscaledTime);

            Scripting::LatentActionHandle InvokeRepeating(
                Scene::Component* owner,
                float initialDelaySeconds,
                float intervalSeconds,
                std::function<void()> callback,
                bool useUnscaledTime);

            void Cancel(Scripting::LatentActionHandle handle);
            void CancelAllForOwner(Scene::Component* owner);

            void Tick(float scaledDeltaTime, float unscaledDeltaTime);

        private:
            Scheduler() = default;

            Scripting::LatentActionHandle ScheduleOnce(
                Scene::Component* owner,
                float delaySeconds,
                std::function<void()> callback,
                bool useUnscaledTime);

            Scripting::LatentActionRunner& SelectRunner(bool useUnscaledTime);
            void TrackHandle(Scene::Component* owner, Scripting::LatentActionHandle handle, bool useUnscaledTime);
            void UntrackHandle(Scripting::LatentActionHandle handle);
            void CancelRepeatingForOwner(Scene::Component* owner);

            struct RepeatingJob {
                std::uint64_t id = 0;
                Scene::Component* owner = nullptr;
                float intervalSeconds = 0.0f;
                bool useUnscaledTime = false;
                std::function<void()> callback;
                Scripting::LatentActionHandle activeHandle;
                bool cancelled = false;
            };

            Scripting::LatentActionRunner scaledRunner;
            Scripting::LatentActionRunner unscaledRunner;

            std::unordered_map<std::uint64_t, Scene::Component*> handleOwners;
            std::unordered_map<std::uint64_t, bool> handleUsesUnscaledTime;

            std::vector<std::shared_ptr<RepeatingJob>> repeatingJobs;
            std::uint64_t nextRepeatingId = 1;
        };

    } // namespace Core
} // namespace RTBEngine
