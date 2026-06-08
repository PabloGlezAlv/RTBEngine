#pragma once

#include "IOnlineBackend.h"
#include "OnlineConfig.h"
#include "OnlineTypes.h"
#include "LanOnlineIdentity.h"
#include "LanOnlineLobby.h"
#include "RelayNetworkTransport.h"
#include "RelayOnlineLobby.h"
#include "UdpNetworkTransport.h"

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        // Runs LAN and Relay lobby stacks together; gameplay transport follows the active lobby session.
        class RTB_API CompositeOnlineBackend final : public IOnlineBackend {
        public:
            CompositeOnlineBackend();

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

            IOnlineLobby* GetLobby(OnlineBackendType lobbyBackend);
            const IOnlineLobby* GetLobby(OnlineBackendType lobbyBackend) const;
            bool IsLobbyBackendReady(OnlineBackendType lobbyBackend) const;
            OnlineBackendType GetActiveLobbyBackend() const;
            OnlineBackendType GetDefaultLobbyBackend() const { return defaultLobbyBackend; }

        private:
            OnlineBackendType ResolveActiveLobbyBackend() const;
            void SyncRelayTransport();

            bool initialized = false;
            bool lanReady = false;
            bool relayReady = false;
            OnlineBackendType defaultLobbyBackend = OnlineBackendType::Lan;
            std::string lastError;
            LanOnlineIdentity identity;
            UdpNetworkTransport lanTransport;
            RelayNetworkTransport relayTransport;
            LanOnlineLobby lanLobby;
            RelayOnlineLobby relayLobby;
        };
#pragma warning(pop)

    }
}
