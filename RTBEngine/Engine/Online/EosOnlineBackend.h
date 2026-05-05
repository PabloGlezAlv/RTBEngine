#pragma once

#include "IOnlineBackend.h"

#include <memory>
#include <string>

namespace RTBEngine {
    namespace Online {

        class EosOnlineIdentity;

#pragma warning(push)
#pragma warning(disable: 4251)
        // EOS SDK backend.
        // This first implementation only owns SDK startup, platform ticking, and shutdown.
        class RTB_API EosOnlineBackend final : public IOnlineBackend {
        public:
            EosOnlineBackend();
            ~EosOnlineBackend() override;

            const char* GetName() const override;
            bool Initialize(const OnlineConfig& config) override;
            void Tick(float deltaTime) override;
            void Shutdown() override;
            bool IsInitialized() const override;
            const char* GetLastError() const override;
            IOnlineIdentity* GetIdentity() override;
            const IOnlineIdentity* GetIdentity() const override;

        private:
            // Stored as void* to keep EOS types out of the public engine header surface.
            void* platformHandle = nullptr;

            // Tracks ownership so we only call EOS_Shutdown if this backend called EOS_Initialize.
            bool eosInitializedByBackend = false;
            bool initialized = false;
            std::string lastError;
            std::unique_ptr<EosOnlineIdentity> identity;
        };
#pragma warning(pop)

    }
}
