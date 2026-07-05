#pragma once

#include "../RTBEngineAPI.h"
#include "Event.h"

namespace RTBEngine {
    namespace Core {

        // Counts down in seconds, emitting UI-friendly second ticks and a finish signal.
        class RTB_API CountdownTimer {
        public:
            using SecondChangedCallback = Event<int>::Callback;
            using FinishedCallback = Event<void>::Callback;

            void Start(float durationSeconds);
            void Stop();
            void Reset();

            bool IsRunning() const { return running; }
            float GetRemaining() const { return remaining; }
            int GetDisplayedSeconds() const;

            EventSubscription SubscribeSecondChanged(SecondChangedCallback callback);
            EventSubscription SubscribeFinished(FinishedCallback callback);

            void Tick(float deltaTime);

        private:
            void EmitDisplayedSecond();
            void Finish();

            float remaining = 0.0f;
            int displayedSeconds = -1;
            bool running = false;
            Event<int> secondChangedEvent;
            Event<void> finishedEvent;
        };

    }
}
