#pragma once

#include "IOnlineBackend.h"

namespace RTBEngine {
    namespace Online {

        // No-op backend used for tests, offline builds, and future editor workflows.
        class RTB_API NullOnlineBackend final : public IOnlineBackend {
        public:
            const char* GetName() const override;
            bool Initialize(const OnlineConfig& config) override;
            void Tick(float deltaTime) override;
            void Shutdown() override;
            bool IsInitialized() const override;
            const char* GetLastError() const override;

        private:
            bool initialized = false;
        };

    }
}
