#pragma once

#include "IOnlineBackend.h"
#include "NullNetworkTransport.h"
#include "NullOnlineIdentity.h"
#include "NullOnlineLobby.h"

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        // No-op backend used for tests, offline builds, and future editor workflows.
        class RTB_API NullOnlineBackend final : public IOnlineBackend {
        public:
            NullOnlineBackend();

            const char* GetName() const override;
            bool Initialize(const OnlineConfig& config) override;
            void Tick(float deltaTime) override;
            void Shutdown() override;
            bool IsInitialized() const override;
            const char* GetLastError() const override;
            IOnlineIdentity* GetIdentity() override;
            const IOnlineIdentity* GetIdentity() const override;
            IOnlineLobby* GetLobby() override;
            const IOnlineLobby* GetLobby() const override;
            IOnlineTransport* GetTransport() override;
            const IOnlineTransport* GetTransport() const override;

        private:
            bool initialized = false;
            NullOnlineIdentity identity;
            NullOnlineLobby lobby;
            NullNetworkTransport transport;
        };
#pragma warning(pop)

    }
}
