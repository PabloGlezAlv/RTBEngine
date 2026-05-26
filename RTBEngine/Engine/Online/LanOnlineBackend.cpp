#include "LanOnlineBackend.h"

#include "OnlineConfig.h"
#include "../Core/Logger.h"

namespace RTBEngine {
    namespace Online {

        LanOnlineBackend::LanOnlineBackend()
            : lobby(&identity, &transport)
        {
        }

        const char* LanOnlineBackend::GetName() const
        {
            return "LAN";
        }

        bool LanOnlineBackend::Initialize(const OnlineConfig& config)
        {
            Shutdown();

            std::string winsockError;
            if (!InitializeWinsock(winsockError)) {
                lastError = winsockError;
                return false;
            }

            std::string bindError;
            if (!transport.Bind(config.lanGamePort, bindError)) {
                lastError = bindError;
                ShutdownWinsock();
                return false;
            }

            if (!lobby.Configure(config.lanDiscoveryPort, config.lanGamePort, bindError)) {
                lastError = bindError;
                transport.Unbind();
                ShutdownWinsock();
                return false;
            }

            initialized = true;
            lastError.clear();
            RTB_INFO("OnlineSystem: LAN backend initialized on game port " +
                std::to_string(config.lanGamePort) + ".");
            return true;
        }

        void LanOnlineBackend::Tick(float deltaTime)
        {
            if (!initialized) {
                return;
            }

            transport.PumpIncoming();
            lobby.Tick(deltaTime);
        }

        void LanOnlineBackend::Shutdown()
        {
            if (initialized) {
                RTB_INFO("OnlineSystem: LAN backend shut down.");
            }

            lobby.LeaveLobby();
            transport.Unbind();
            initialized = false;
            ShutdownWinsock();
        }

        bool LanOnlineBackend::IsInitialized() const
        {
            return initialized;
        }

        const char* LanOnlineBackend::GetLastError() const
        {
            return lastError.c_str();
        }

        IOnlineIdentity* LanOnlineBackend::GetIdentity()
        {
            return &identity;
        }

        const IOnlineIdentity* LanOnlineBackend::GetIdentity() const
        {
            return &identity;
        }

        IOnlineLobby* LanOnlineBackend::GetLobby()
        {
            return &lobby;
        }

        const IOnlineLobby* LanOnlineBackend::GetLobby() const
        {
            return &lobby;
        }

        IOnlineTransport* LanOnlineBackend::GetTransport()
        {
            return &transport;
        }

        const IOnlineTransport* LanOnlineBackend::GetTransport() const
        {
            return &transport;
        }

    }
}
