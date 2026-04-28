#include "Time.h"

#include <algorithm>

namespace {
    struct TimeState {
        float deltaTime = 0.0f;
        float unscaledDeltaTime = 0.0f;
        float time = 0.0f;
        float unscaledTime = 0.0f;
        float timeScale = 1.0f;
        float fixedDeltaTime = 1.0f / 60.0f;
        float fixedAccumulator = 0.0f;
        std::uint64_t frameCount = 0;
        bool paused = false;
    };

    TimeState& State()
    {
        static TimeState state;
        return state;
    }
}

namespace RTBEngine {
    namespace Core {

        void Time::Reset()
        {
            TimeState& state = State();
            const float fixedDeltaTime = state.fixedDeltaTime;
            state = {};
            state.fixedDeltaTime = fixedDeltaTime;
        }

        void Time::AdvanceFrame(float rawDeltaTime)
        {
            TimeState& state = State();
            state.unscaledDeltaTime = std::max(0.0f, rawDeltaTime);
            state.deltaTime = state.paused
                ? 0.0f
                : state.unscaledDeltaTime * std::max(0.0f, state.timeScale);

            state.unscaledTime += state.unscaledDeltaTime;
            state.time += state.deltaTime;

            if (!state.paused) {
                state.fixedAccumulator += state.deltaTime;
            }

            ++state.frameCount;
        }

        void Time::SetPaused(bool paused)
        {
            TimeState& state = State();
            state.paused = paused;
            if (paused) {
                state.deltaTime = 0.0f;
                state.fixedAccumulator = 0.0f;
            }
        }

        bool Time::IsPaused()
        {
            return State().paused;
        }

        void Time::SetTimeScale(float scale)
        {
            State().timeScale = std::max(0.0f, scale);
        }

        float Time::GetTimeScale()
        {
            return State().timeScale;
        }

        float Time::GetDeltaTime()
        {
            return State().deltaTime;
        }

        float Time::GetUnscaledDeltaTime()
        {
            return State().unscaledDeltaTime;
        }

        float Time::GetTime()
        {
            return State().time;
        }

        float Time::GetUnscaledTime()
        {
            return State().unscaledTime;
        }

        void Time::SetFixedDeltaTime(float fixedDeltaTime)
        {
            State().fixedDeltaTime = std::max(0.0f, fixedDeltaTime);
        }

        float Time::GetFixedDeltaTime()
        {
            return State().fixedDeltaTime;
        }

        bool Time::HasFixedStep()
        {
            const TimeState& state = State();
            return state.fixedDeltaTime > 0.0f &&
                state.fixedAccumulator + 0.000001f >= state.fixedDeltaTime;
        }

        bool Time::ConsumeFixedStep()
        {
            TimeState& state = State();
            if (!HasFixedStep()) {
                return false;
            }

            state.fixedAccumulator = std::max(0.0f, state.fixedAccumulator - state.fixedDeltaTime);
            return true;
        }

        float Time::GetFixedInterpolationAlpha()
        {
            const TimeState& state = State();
            if (state.fixedDeltaTime <= 0.0f) {
                return 1.0f;
            }

            return std::clamp(state.fixedAccumulator / state.fixedDeltaTime, 0.0f, 1.0f);
        }

        std::uint64_t Time::GetFrameCount()
        {
            return State().frameCount;
        }

    }
}
