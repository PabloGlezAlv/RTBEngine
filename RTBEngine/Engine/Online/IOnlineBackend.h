#pragma once

#include "../RTBEngineAPI.h"

namespace RTBEngine {
    namespace Online {

        class IOnlineIdentity;
        class IOnlineLobby;
        class IOnlineTransport;
        struct OnlineConfig;

        class RTB_API IOnlineBackend {
        public:
            virtual ~IOnlineBackend() = default;

            virtual const char* GetName() const = 0;
            virtual bool Initialize(const OnlineConfig& config) = 0;
            virtual void Tick(float deltaTime) = 0;
            virtual void Shutdown() = 0;
            virtual bool IsInitialized() const = 0;
            virtual const char* GetLastError() const = 0;
            // Local user login/session; nullptr only if backend failed to init.
            virtual IOnlineIdentity* GetIdentity() = 0;
            virtual const IOnlineIdentity* GetIdentity() const = 0;
            // Lobby create/join/search; nullptr only if backend failed to init.
            virtual IOnlineLobby* GetLobby() = 0;
            virtual const IOnlineLobby* GetLobby() const = 0;
            // Gameplay packet send/receive; nullptr only if backend failed to init.
            virtual IOnlineTransport* GetTransport() = 0;
            virtual const IOnlineTransport* GetTransport() const = 0;
        };

    }
}
