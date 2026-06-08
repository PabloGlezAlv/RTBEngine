#include "CompositeOnlineBackend.h"

#include "OnlineConfig.h"
#include "UdpSocket.h"
#include "../Core/Logger.h"

namespace RTBEngine {
    namespace Online {

        CompositeOnlineBackend::CompositeOnlineBackend()
            : lanLobby(&identity, &lanTransport)
            , relayLobby(&identity)
        {
        }

        const char* CompositeOnlineBackend::GetName() const
        {
            return "LAN+Relay";
        }

        bool CompositeOnlineBackend::Initialize(const OnlineConfig& config)
        {
            Shutdown();

            defaultLobbyBackend = config.backendType;
            lastError.clear();
            lanReady = false;
            relayReady = false;

            std::string winsockError;
            if (!InitializeWinsock(winsockError)) {
                lastError = winsockError;
                return false;
            }

            std::string bindError;
            if (!lanTransport.Bind(config.lanGamePort, bindError)) {
                lastError = bindError;
                ShutdownWinsock();
                return false;
            }

            if (!lanLobby.Configure(config.lanDiscoveryPort, config.lanGamePort, bindError)) {
                lastError = bindError;
                lanTransport.Unbind();
                ShutdownWinsock();
                return false;
            }

            lanReady = true;

            if (!config.relayMatchmakingUrl.empty()) {
                if (!relayLobby.Configure(config.relayMatchmakingUrl, bindError)) {
                    RTB_WARN("CompositeOnlineBackend: relay lobby unavailable. " + bindError);
                } else {
                    relayReady = true;
                }
            } else {
                RTB_INFO("CompositeOnlineBackend: relay matchmaking URL is empty; relay lobby disabled.");
            }

            initialized = true;
            RTB_INFO(
                std::string("CompositeOnlineBackend: LAN ready. Relay ") +
                (relayReady ? "ready." : "not configured."));
            return true;
        }

        void CompositeOnlineBackend::Tick(float deltaTime)
        {
            if (!initialized) {
                return;
            }

            lanTransport.PumpIncoming();
            if (lanReady) {
                lanLobby.Tick(deltaTime);
            }

            if (relayReady) {
                relayLobby.Tick(deltaTime);
                SyncRelayTransport();
                if (relayTransport.IsConnected()) {
                    relayTransport.PumpIncoming();
                    relayTransport.TickKeepalive(deltaTime);
                }
            } else {
                relayTransport.Disconnect();
            }
        }

        void CompositeOnlineBackend::Shutdown()
        {
            if (lanReady) {
                lanLobby.LeaveLobby();
            }

            if (relayReady) {
                relayLobby.LeaveLobby();
            }

            relayTransport.Disconnect();
            lanTransport.Unbind();
            initialized = false;
            lanReady = false;
            relayReady = false;
            ShutdownWinsock();
        }

        bool CompositeOnlineBackend::IsInitialized() const
        {
            return initialized;
        }

        const char* CompositeOnlineBackend::GetLastError() const
        {
            return lastError.c_str();
        }

        IOnlineIdentity* CompositeOnlineBackend::GetIdentity()
        {
            return &identity;
        }

        const IOnlineIdentity* CompositeOnlineBackend::GetIdentity() const
        {
            return &identity;
        }

        IOnlineLobby* CompositeOnlineBackend::GetLobby(OnlineBackendType lobbyBackend)
        {
            if (IsRelayBackend(lobbyBackend)) {
                if (relayReady) {
                    return &relayLobby;
                }
                return nullptr;
            }

            if (lanReady) {
                return &lanLobby;
            }
            return nullptr;
        }

        const IOnlineLobby* CompositeOnlineBackend::GetLobby(OnlineBackendType lobbyBackend) const
        {
            return const_cast<CompositeOnlineBackend*>(this)->GetLobby(lobbyBackend);
        }

        IOnlineLobby* CompositeOnlineBackend::GetLobby()
        {
            return GetLobby(ResolveActiveLobbyBackend());
        }

        const IOnlineLobby* CompositeOnlineBackend::GetLobby() const
        {
            return GetLobby(ResolveActiveLobbyBackend());
        }

        IOnlineTransport* CompositeOnlineBackend::GetTransport()
        {
            if (IsRelayBackend(ResolveActiveLobbyBackend())) {
                return relayTransport.IsConnected() ? &relayTransport : nullptr;
            }

            if (lanReady) {
                return &lanTransport;
            }
            return nullptr;
        }

        const IOnlineTransport* CompositeOnlineBackend::GetTransport() const
        {
            return const_cast<CompositeOnlineBackend*>(this)->GetTransport();
        }

        bool CompositeOnlineBackend::IsLobbyBackendReady(OnlineBackendType lobbyBackend) const
        {
            if (IsRelayBackend(lobbyBackend)) {
                return relayReady;
            }

            return lanReady;
        }

        OnlineBackendType CompositeOnlineBackend::GetActiveLobbyBackend() const
        {
            return ResolveActiveLobbyBackend();
        }

        OnlineBackendType CompositeOnlineBackend::ResolveActiveLobbyBackend() const
        {
            const bool lanInLobby = lanReady && !lanLobby.GetCurrentLobby().lobbyId.empty();
            const bool relayInLobby = relayReady && !relayLobby.GetCurrentLobby().lobbyId.empty();

            if (lanInLobby && !relayInLobby) {
                return OnlineBackendType::Lan;
            }

            if (relayInLobby && !lanInLobby) {
                return OnlineBackendType::RelayOnline;
            }

            if (relayInLobby && lanInLobby) {
                RTB_WARN("CompositeOnlineBackend: both LAN and Relay lobbies are active; using default backend.");
            }

            return defaultLobbyBackend;
        }

        void CompositeOnlineBackend::SyncRelayTransport()
        {
            const bool inRelayLobby =
                relayLobby.GetState() == OnlineLobbyState::InLobby &&
                !relayLobby.GetCurrentLobby().lobbyId.empty();

            if (!inRelayLobby) {
                relayTransport.Disconnect();
                return;
            }

            if (relayTransport.IsConnected()) {
                return;
            }

            std::string connectError;
            if (!relayTransport.Connect(relayLobby.GetRelaySessionInfo(), connectError)) {
                RTB_WARN("CompositeOnlineBackend: relay transport connect failed. " + connectError);
            }
        }

    }
}
