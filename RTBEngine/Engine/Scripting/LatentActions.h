#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace RTBEngine {
    namespace Scripting {

        struct LatentActionHandle {
            std::uint64_t id = 0;

            bool IsValid() const
            {
                return id != 0;
            }

            explicit operator bool() const
            {
                return IsValid();
            }
        };

        inline bool operator==(const LatentActionHandle& lhs, const LatentActionHandle& rhs)
        {
            return lhs.id == rhs.id;
        }

        inline bool operator!=(const LatentActionHandle& lhs, const LatentActionHandle& rhs)
        {
            return !(lhs == rhs);
        }

        class LatentSequence {
        public:
            LatentSequence& Call(std::function<void()> callback)
            {
                steps.push_back(Step::MakeCall(std::move(callback)));
                return *this;
            }

            LatentSequence& Wait(float seconds)
            {
                steps.push_back(Step::MakeWait(seconds));
                return *this;
            }

            LatentSequence& Tween(float durationSec, std::function<void(float)> onUpdate, std::function<void()> onComplete = {})
            {
                steps.push_back(Step::MakeTween(durationSec, std::move(onUpdate), std::move(onComplete)));
                return *this;
            }

            bool Empty() const
            {
                return steps.empty();
            }

        private:
            enum class StepType {
                Call,
                Wait,
                Tween
            };

            struct Step {
                StepType type = StepType::Call;
                float durationSec = 0.0f;
                std::function<void()> call;
                std::function<void(float)> onUpdate;
                std::function<void()> onComplete;

                static Step MakeCall(std::function<void()> callback)
                {
                    Step step;
                    step.type = StepType::Call;
                    step.call = std::move(callback);
                    return step;
                }

                static Step MakeWait(float seconds)
                {
                    Step step;
                    step.type = StepType::Wait;
                    step.durationSec = std::max(0.0f, seconds);
                    return step;
                }

                static Step MakeTween(float durationSec, std::function<void(float)> onUpdateCallback, std::function<void()> onCompleteCallback)
                {
                    Step step;
                    step.type = StepType::Tween;
                    step.durationSec = std::max(0.0f, durationSec);
                    step.onUpdate = std::move(onUpdateCallback);
                    step.onComplete = std::move(onCompleteCallback);
                    return step;
                }
            };

            friend class LatentActionRunner;

            std::vector<Step> steps;
        };

        class LatentActionRunner {
        public:
            LatentActionHandle Play(LatentSequence sequence)
            {
                if (sequence.Empty()) {
                    return {};
                }

                const LatentActionHandle handle{ nextHandleId++ };

                ActiveAction action;
                action.handle = handle;
                action.sequence = std::move(sequence);

                if (isTicking) {
                    // Callbacks may schedule more work while we are iterating.
                    // Queue new actions first so the active list stays stable.
                    pendingAdds.push_back(std::move(action));
                } else {
                    actions.push_back(std::move(action));
                }

                return handle;
            }

            void Stop(LatentActionHandle handle)
            {
                if (!handle.IsValid()) {
                    return;
                }

                MarkStopped(actions, handle);
                MarkStopped(pendingAdds, handle);

                if (!isTicking) {
                    CleanupActions();
                }
            }

            void StopAll()
            {
                MarkAllStopped(actions);
                MarkAllStopped(pendingAdds);

                if (!isTicking) {
                    CleanupActions();
                }
            }

            bool HasActiveActions() const
            {
                return HasLiveActions(actions) || HasLiveActions(pendingAdds);
            }

            void Tick(float deltaTime)
            {
                if (actions.empty() && pendingAdds.empty()) {
                    return;
                }

                if (!pendingAdds.empty()) {
                    FlushPendingAdds();
                }

                isTicking = true;
                // Negative delta is treated as "no time passed" so step
                // progression remains monotonic and deterministic.
                const float safeDeltaTime = std::max(0.0f, deltaTime);

                for (ActiveAction& action : actions) {
                    TickAction(action, safeDeltaTime);
                }

                isTicking = false;

                FlushPendingAdds();
                CleanupActions();
            }

        private:
            struct ActiveAction {
                LatentActionHandle handle;
                LatentSequence sequence;
                std::size_t stepIndex = 0;
                float stepElapsed = 0.0f;
                bool stopped = false;
                bool completed = false;
            };

            // Guards against pathological same-frame loops caused by long chains
            // of instant-complete steps.
            static constexpr std::size_t kMaxStepAdvancesPerTick = 256;

            static bool IsLiveAction(const ActiveAction& action)
            {
                return !action.stopped && !action.completed;
            }

            static bool HasLiveActions(const std::vector<ActiveAction>& actions)
            {
                for (const ActiveAction& action : actions) {
                    if (IsLiveAction(action)) {
                        return true;
                    }
                }
                return false;
            }

            static void MarkStopped(std::vector<ActiveAction>& actions, LatentActionHandle handle)
            {
                for (ActiveAction& action : actions) {
                    if (action.handle == handle) {
                        action.stopped = true;
                    }
                }
            }

            static void MarkAllStopped(std::vector<ActiveAction>& actions)
            {
                for (ActiveAction& action : actions) {
                    action.stopped = true;
                }
            }

            void TickAction(ActiveAction& action, float deltaTime)
            {
                if (!IsLiveAction(action)) {
                    return;
                }

                float remainingDelta = deltaTime;
                std::size_t safetyCounter = 0;

                while (IsLiveAction(action) && safetyCounter < kMaxStepAdvancesPerTick) {
                    if (action.stepIndex >= action.sequence.steps.size()) {
                        action.completed = true;
                        return;
                    }

                    const bool advancedStep = ExecuteCurrentStep(action, remainingDelta);
                    if (!advancedStep) {
                        return;
                    }

                    ++safetyCounter;
                }

                if (safetyCounter >= kMaxStepAdvancesPerTick) {
                    action.completed = true;
                }
            }

            bool ExecuteCurrentStep(ActiveAction& action, float& remainingDelta)
            {
                const LatentSequence::Step& step = action.sequence.steps[action.stepIndex];

                switch (step.type) {
                case LatentSequence::StepType::Call:
                    if (step.call) {
                        step.call();
                    }
                    if (action.stopped) {
                        return false;
                    }
                    AdvanceStep(action);
                    return true;

                case LatentSequence::StepType::Wait:
                    return ExecuteWaitStep(action, step, remainingDelta);

                case LatentSequence::StepType::Tween:
                    return ExecuteTweenStep(action, step, remainingDelta);
                }

                return false;
            }

            bool ExecuteWaitStep(ActiveAction& action, const LatentSequence::Step& step, float& remainingDelta)
            {
                if (step.durationSec <= 0.0f) {
                    AdvanceStep(action);
                    return true;
                }

                action.stepElapsed += remainingDelta;
                if (action.stepElapsed < step.durationSec) {
                    remainingDelta = 0.0f;
                    return false;
                }

                // Carry overshoot into the next step so long waits remain
                // frame-rate independent.
                remainingDelta = std::max(0.0f, action.stepElapsed - step.durationSec);
                AdvanceStep(action);
                return true;
            }

            bool ExecuteTweenStep(ActiveAction& action, const LatentSequence::Step& step, float& remainingDelta)
            {
                if (step.durationSec <= 0.0f) {
                    // Zero-length tweens still emit a final update so callers can
                    // snap to the exact target in a single tick.
                    if (step.onUpdate) {
                        step.onUpdate(1.0f);
                    }
                    if (action.stopped) {
                        return false;
                    }
                    if (step.onComplete) {
                        step.onComplete();
                    }
                    if (action.stopped) {
                        return false;
                    }
                    AdvanceStep(action);
                    return true;
                }

                action.stepElapsed += remainingDelta;

                if (step.onUpdate) {
                    const float progress = std::min(1.0f, action.stepElapsed / step.durationSec);
                    step.onUpdate(progress);
                }
                if (action.stopped) {
                    return false;
                }

                if (action.stepElapsed < step.durationSec) {
                    remainingDelta = 0.0f;
                    return false;
                }

                remainingDelta = std::max(0.0f, action.stepElapsed - step.durationSec);

                if (step.onComplete) {
                    step.onComplete();
                }
                if (action.stopped) {
                    return false;
                }

                AdvanceStep(action);
                return true;
            }

            static void AdvanceStep(ActiveAction& action)
            {
                ++action.stepIndex;
                action.stepElapsed = 0.0f;
                if (action.stepIndex >= action.sequence.steps.size()) {
                    action.completed = true;
                }
            }

            void FlushPendingAdds()
            {
                if (pendingAdds.empty()) {
                    return;
                }

                actions.insert(actions.end(),
                    std::make_move_iterator(pendingAdds.begin()),
                    std::make_move_iterator(pendingAdds.end()));
                pendingAdds.clear();
            }

            void CleanupActions()
            {
                CleanupActionList(actions);
                CleanupActionList(pendingAdds);
            }

            static void CleanupActionList(std::vector<ActiveAction>& actions)
            {
                actions.erase(
                    std::remove_if(actions.begin(), actions.end(),
                        [](const ActiveAction& action) {
                            return action.stopped || action.completed;
                        }),
                    actions.end());
            }

            std::vector<ActiveAction> actions;
            // Actions requested during callback execution are staged here and
            // merged back once the current tick is finished.
            std::vector<ActiveAction> pendingAdds;
            std::uint64_t nextHandleId = 1;
            bool isTicking = false;
        };

    }
}
