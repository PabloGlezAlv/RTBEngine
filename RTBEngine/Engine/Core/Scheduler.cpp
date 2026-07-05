#include "Scheduler.h"

#include "../Scene/Component.h"
#include "../Scene/GameObject.h"

#include <algorithm>
#include <memory>

namespace RTBEngine {
    namespace Core {

        Scheduler& Scheduler::GetInstance()
        {
            static Scheduler instance;
            return instance;
        }

        Scripting::LatentActionRunner& Scheduler::SelectRunner(bool useUnscaledTime)
        {
            return useUnscaledTime ? unscaledRunner : scaledRunner;
        }

        void Scheduler::TrackHandle(
            ECS::Component* owner,
            Scripting::LatentActionHandle handle,
            bool useUnscaledTime)
        {
            if (!handle.IsValid()) {
                return;
            }

            handleOwners[handle.id] = owner;
            handleUsesUnscaledTime[handle.id] = useUnscaledTime;
        }

        void Scheduler::UntrackHandle(Scripting::LatentActionHandle handle)
        {
            if (!handle.IsValid()) {
                return;
            }

            handleOwners.erase(handle.id);
            handleUsesUnscaledTime.erase(handle.id);
        }

        Scripting::LatentActionHandle Scheduler::StartSequence(
            ECS::Component* owner,
            Scripting::LatentSequence sequence,
            bool useUnscaledTime)
        {
            if (sequence.Empty()) {
                return {};
            }

            Scripting::LatentActionHandle handle = SelectRunner(useUnscaledTime).Play(std::move(sequence));
            TrackHandle(owner, handle, useUnscaledTime);
            return handle;
        }

        Scripting::LatentActionHandle Scheduler::ScheduleOnce(
            ECS::Component* owner,
            float delaySeconds,
            std::function<void()> callback,
            bool useUnscaledTime)
        {
            if (!callback) {
                return {};
            }

            Scripting::LatentSequence sequence;
            sequence.Wait(std::max(0.0f, delaySeconds)).Call(std::move(callback));
            return StartSequence(owner, std::move(sequence), useUnscaledTime);
        }

        Scripting::LatentActionHandle Scheduler::Invoke(
            ECS::Component* owner,
            float delaySeconds,
            std::function<void()> callback,
            bool useUnscaledTime)
        {
            return ScheduleOnce(owner, delaySeconds, std::move(callback), useUnscaledTime);
        }

        Scripting::LatentActionHandle Scheduler::InvokeRepeating(
            ECS::Component* owner,
            float initialDelaySeconds,
            float intervalSeconds,
            std::function<void()> callback,
            bool useUnscaledTime)
        {
            if (!callback) {
                return {};
            }

            auto job = std::make_shared<RepeatingJob>();
            job->id = nextRepeatingId++;
            job->owner = owner;
            job->intervalSeconds = std::max(0.0f, intervalSeconds);
            job->useUnscaledTime = useUnscaledTime;
            job->callback = std::move(callback);

            const std::shared_ptr<RepeatingJob> jobRef = job;
            const auto tickFn = std::make_shared<std::function<void()>>();
            *tickFn = [this, jobRef, tickFn]() {
                if (!jobRef || jobRef->cancelled) {
                    return;
                }

                if (ECS::Component* component = jobRef->owner) {
                    if (!component->IsEnabled() || !component->GetOwner() ||
                        !component->GetOwner()->IsActiveInHierarchy()) {
                        jobRef->cancelled = true;
                        return;
                    }
                }

                if (jobRef->callback) {
                    jobRef->callback();
                }

                if (jobRef->cancelled) {
                    return;
                }

                jobRef->activeHandle = ScheduleOnce(
                    jobRef->owner,
                    jobRef->intervalSeconds,
                    [tickFn]() {
                        if (tickFn && *tickFn) {
                            (*tickFn)();
                        }
                    },
                    jobRef->useUnscaledTime);
            };

            job->activeHandle = ScheduleOnce(
                job->owner,
                initialDelaySeconds,
                [tickFn]() {
                    if (tickFn && *tickFn) {
                        (*tickFn)();
                    }
                },
                useUnscaledTime);

            repeatingJobs.push_back(std::move(job));
            return job->activeHandle;
        }

        void Scheduler::Cancel(Scripting::LatentActionHandle handle)
        {
            if (!handle.IsValid()) {
                return;
            }

            const auto timeIt = handleUsesUnscaledTime.find(handle.id);
            if (timeIt != handleUsesUnscaledTime.end()) {
                SelectRunner(timeIt->second).Stop(handle);
            } else {
                scaledRunner.Stop(handle);
                unscaledRunner.Stop(handle);
            }

            UntrackHandle(handle);

            for (const std::shared_ptr<RepeatingJob>& job : repeatingJobs) {
                if (job && job->activeHandle == handle) {
                    job->cancelled = true;
                }
            }
        }

        void Scheduler::CancelRepeatingForOwner(ECS::Component* owner)
        {
            for (const std::shared_ptr<RepeatingJob>& job : repeatingJobs) {
                if (job && job->owner == owner) {
                    job->cancelled = true;
                    if (job->activeHandle.IsValid()) {
                        Cancel(job->activeHandle);
                    }
                }
            }
        }

        void Scheduler::CancelAllForOwner(ECS::Component* owner)
        {
            if (!owner) {
                return;
            }

            CancelRepeatingForOwner(owner);

            std::vector<Scripting::LatentActionHandle> handlesToCancel;
            handlesToCancel.reserve(handleOwners.size());

            for (const auto& entry : handleOwners) {
                if (entry.second == owner) {
                    handlesToCancel.push_back(Scripting::LatentActionHandle{ entry.first });
                }
            }

            for (Scripting::LatentActionHandle handle : handlesToCancel) {
                Cancel(handle);
            }
        }

        void Scheduler::Tick(float scaledDeltaTime, float unscaledDeltaTime)
        {
            scaledRunner.Tick(scaledDeltaTime);
            unscaledRunner.Tick(unscaledDeltaTime);

            repeatingJobs.erase(
                std::remove_if(repeatingJobs.begin(), repeatingJobs.end(),
                    [](const std::shared_ptr<RepeatingJob>& job) {
                        return !job || job->cancelled;
                    }),
                repeatingJobs.end());
        }

    } // namespace Core
} // namespace RTBEngine
