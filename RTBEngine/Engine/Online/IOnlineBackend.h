#pragma once

#include "../RTBEngineAPI.h"

namespace RTBEngine {
    namespace Online {

        struct OnlineConfig;

        // Backend contract for the online layer.
        class RTB_API IOnlineBackend {
        public:
            virtual ~IOnlineBackend() = default;

            virtual const char* GetName() const = 0;
            virtual bool Initialize(const OnlineConfig& config) = 0;
            virtual void Tick(float deltaTime) = 0;
            virtual void Shutdown() = 0;
            virtual bool IsInitialized() const = 0;
            virtual const char* GetLastError() const = 0;
        };

    }
}
