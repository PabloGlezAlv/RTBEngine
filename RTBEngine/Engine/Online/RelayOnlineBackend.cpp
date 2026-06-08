#include "RelayOnlineBackend.h"

#include "OnlineConfig.h"
#include "../Core/Logger.h"

namespace RTBEngine {
    namespace Online {

        RelayOnlineBackend::RelayOnlineBackend()
            : lobby(&identity)
        {
        }

        const char* RelayOnlineBackend::GetName() const
        {
            return "Relay";
        }

        bool RelayOnlineBackend::Initialize(const OnlineConfig& config)
        {
            Shutdown();

            std::string configureError;
            if (!lobby.Configure(config.relayMatchmakingUrl, configureError)) {
                lastError = configureError;
                return false;
            }

            initialized = true;
            lastError.clear();
            RTB_INFO("OnlineSystem: Relay backend initialized. Matchmaking URL: " + config.relayMatchmakingUrl + ".");
            return true;
        }

        void RelayOnlineBackend::Tick(float deltaTime)
        {
            if (!initialized) {
                return;
            }

            lobby.Tick(deltaTime);
        }

        void RelayOnlineBackend::Shutdown()
        {
            if (initialized) {
                RTB_INFO("OnlineSystem: Relay backend shut down.");
            }

            lobby.LeaveLobby();
            initialized = false;
        }

        bool RelayOnlineBackend::IsInitialized() const
        {
            return initialized;
        }

        const char* RelayOnlineBackend::GetLastError() const
        {
            return lastError.c_str();
        }

        IOnlineIdentity* RelayOnlineBackend::GetIdentity()
        {
            return &identity;
        }

        const IOnlineIdentity* RelayOnlineBackend::GetIdentity() const
        {
            return &identity;
        }

        IOnlineLobby* RelayOnlineBackend::GetLobby()
        {
            return &lobby;
        }

        const IOnlineLobby* RelayOnlineBackend::GetLobby() const
        {
            return &lobby;
        }

        IOnlineTransport* RelayOnlineBackend::GetTransport()
        {
            return nullptr;
        }

        const IOnlineTransport* RelayOnlineBackend::GetTransport() const
        {
            return nullptr;
        }

    }
}
