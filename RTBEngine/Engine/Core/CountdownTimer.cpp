#include "CountdownTimer.h"

#include <algorithm>
#include <cmath>

namespace RTBEngine {
    namespace Core {

        int CountdownTimer::GetDisplayedSeconds() const
        {
            return static_cast<int>(std::ceil(remaining));
        }

        void CountdownTimer::Start(float durationSeconds)
        {
            remaining = std::max(0.0f, durationSeconds);
            displayedSeconds = -1;

            if (remaining <= 0.0f) {
                running = false;
                EmitDisplayedSecond();
                finishedEvent.Invoke();
                return;
            }

            running = true;
            EmitDisplayedSecond();
        }

        void CountdownTimer::Stop()
        {
            running = false;
        }

        void CountdownTimer::Reset()
        {
            running = false;
            remaining = 0.0f;
            displayedSeconds = -1;
        }

        EventSubscription CountdownTimer::SubscribeSecondChanged(SecondChangedCallback callback)
        {
            return secondChangedEvent.Subscribe(std::move(callback));
        }

        EventSubscription CountdownTimer::SubscribeFinished(FinishedCallback callback)
        {
            return finishedEvent.Subscribe(std::move(callback));
        }

        void CountdownTimer::Tick(float deltaTime)
        {
            if (!running) {
                return;
            }

            remaining = std::max(0.0f, remaining - std::max(0.0f, deltaTime));

            const int seconds = GetDisplayedSeconds();
            if (seconds != displayedSeconds) {
                displayedSeconds = seconds;
                secondChangedEvent.Invoke(seconds);
            }

            if (remaining <= 0.0f) {
                Finish();
            }
        }

        void CountdownTimer::EmitDisplayedSecond()
        {
            const int seconds = GetDisplayedSeconds();
            if (seconds == displayedSeconds) {
                return;
            }

            displayedSeconds = seconds;
            secondChangedEvent.Invoke(seconds);
        }

        void CountdownTimer::Finish()
        {
            running = false;
            remaining = 0.0f;
            finishedEvent.Invoke();
        }

    }
}
