#pragma once

#include "../RTBEngineAPI.h"
#include <cstdint>

namespace RTBEngine {
    namespace Core {

        class RTB_API Time {
        public:
            static void Reset();
            static void AdvanceFrame(float rawDeltaTime);

            static void SetPaused(bool paused);
            static bool IsPaused();

            static void SetTimeScale(float scale);
            static float GetTimeScale();

            static float GetDeltaTime();
            static float GetUnscaledDeltaTime();
            static float GetTime();
            static float GetUnscaledTime();

            static void SetFixedDeltaTime(float fixedDeltaTime);
            static float GetFixedDeltaTime();
            static bool HasFixedStep();
            static bool ConsumeFixedStep();
            static float GetFixedInterpolationAlpha();

            static std::uint64_t GetFrameCount();

        private:
            Time() = delete;
        };

    }
}
