#pragma once

#include "IOnlineBackend.h"
#include "LanOnlineLobby.h"
#include "NullOnlineIdentity.h"
#include "UdpNetworkTransport.h"

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API LanOnlineBackend final : public IOnlineBackend {
        public:
            LanOnlineBackend();
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
            std::string lastError;
            NullOnlineIdentity identity;
            UdpNetworkTransport transport;
            LanOnlineLobby lobby;
        };
#pragma warning(pop)

    }
}
