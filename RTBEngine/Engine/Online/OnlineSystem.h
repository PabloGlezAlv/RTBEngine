#pragma once

#include "../RTBEngineAPI.h"
#include "IOnlineBackend.h"
#include "IOnlineIdentity.h"
#include "IOnlineLobby.h"
#include "IOnlineTransport.h"
#include "OnlineTypes.h"
#include "OnlineUser.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace RTBEngine {
    namespace Online {

        struct OnlineConfig;
#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API OnlineSystem {
        public:
            static OnlineSystem& GetInstance();

            bool Initialize(const OnlineConfig& config);
            void Tick(float deltaTime);
            void Shutdown();

            bool IsEnabled() const { return enabled; }
            bool IsInitialized() const { return state == OnlineState::Initialized; }
            OnlineState GetState() const { return state; }
            OnlineBackendType GetBackendType() const { return OnlineBackendType::LAN; }
            const std::string& GetLastError() const { return lastError; }
            const OnlineLoginOptions& GetDefaultLoginOptions() const { return defaultLoginOptions; }

            IOnlineIdentity* GetIdentity();
            const IOnlineIdentity* GetIdentity() const;
            IOnlineLobby* GetLobby();
            const IOnlineLobby* GetLobby() const;
            IOnlineTransport* GetTransport();
            const IOnlineTransport* GetTransport() const;

            bool IsInLobby() const;
            bool IsLobbyOwner() const;
            OnlineUserId GetLocalUserId() const;
            std::vector<OnlineUserId> GetOrderedLobbyMembers() const;
            std::size_t GetLocalPlayerIndex() const;

        private:
            OnlineSystem() = default;
            ~OnlineSystem();

            OnlineSystem(const OnlineSystem&) = delete;
            OnlineSystem& operator=(const OnlineSystem&) = delete;

            bool enabled = false;
            bool failApplicationOnError = false;
            OnlineState state = OnlineState::Disabled;
            OnlineLoginOptions defaultLoginOptions;
            std::string lastError;
            std::unique_ptr<IOnlineBackend> backend;
        };
#pragma warning(pop)

    }
}
