#pragma once

#include "../RTBEngineAPI.h"
#include "IOnlineBackend.h"
#include "IOnlineIdentity.h"
#include "IOnlineLobby.h"
#include "IOnlineTransport.h"
#include "OnlineTypes.h"
#include "OnlineUser.h"
#include "OnlinePlayerProfile.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
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
            OnlineBackendType GetDefaultLobbyBackend() const { return defaultLobbyBackend; }
            OnlineBackendType GetActiveLobbyBackend() const;
            OnlineBackendType GetBackendType() const { return GetActiveLobbyBackend(); }
            const char* GetActiveBackendName() const;
            bool IsLobbyBackendReady(OnlineBackendType backend) const;
            const std::string& GetLastError() const { return lastError; }
            const OnlineLoginOptions& GetDefaultLoginOptions() const { return defaultLoginOptions; }
            void SetSessionDisplayName(const std::string& name);
            const std::string& GetSessionDisplayName() const;

            IOnlineIdentity* GetIdentity();
            const IOnlineIdentity* GetIdentity() const;
            IOnlineLobby* GetLobby();
            const IOnlineLobby* GetLobby() const;
            IOnlineLobby* GetLobby(OnlineBackendType backend);
            const IOnlineLobby* GetLobby(OnlineBackendType backend) const;
            IOnlineTransport* GetTransport();
            const IOnlineTransport* GetTransport() const;

            bool IsInLobby() const;
            bool IsLobbyOwner() const;
            OnlineUserId GetLocalUserId() const;
            std::vector<OnlineUserId> GetOrderedLobbyMembers() const;
            std::size_t GetLocalPlayerIndex() const;
            std::string GetLobbyMemberDisplayName(const OnlineUserId& member) const;

            void ClearPlayerSessionProfiles();
            void SetPlayerSessionProfile(const OnlinePlayerProfile& profile);
            bool HasPlayerSessionProfile(int playerSlot) const;
            bool TryGetPlayerSessionProfile(int playerSlot, OnlinePlayerProfile& outProfile) const;
            std::string GetPlayerDisplayName(int playerSlot) const;
            std::vector<OnlinePlayerProfile> GetPlayerSessionProfiles() const;

        private:
            OnlineSystem() = default;
            ~OnlineSystem();

            OnlineSystem(const OnlineSystem&) = delete;
            OnlineSystem& operator=(const OnlineSystem&) = delete;

            bool enabled = false;
            bool failApplicationOnError = false;
            OnlineBackendType defaultLobbyBackend = OnlineBackendType::Lan;
            OnlineState state = OnlineState::Disabled;
            OnlineLoginOptions defaultLoginOptions;
            std::string lastError;
            std::unique_ptr<IOnlineBackend> backend;
            std::unordered_map<int, OnlinePlayerProfile> playerSessionProfiles;
        };
#pragma warning(pop)

    }
}
