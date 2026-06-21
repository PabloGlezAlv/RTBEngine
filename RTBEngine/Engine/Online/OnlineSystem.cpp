#include "OnlineSystem.h"

#include "CompositeOnlineBackend.h"
#include "IOnlineBackend.h"
#include "OnlineConfig.h"
#include "OnlineGameplayNet.h"
#include "../Core/Logger.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

namespace RTBEngine {
    namespace Online {

        namespace {

            constexpr std::size_t kMaxSessionDisplayNameLength = 24;

            std::string SanitizeSessionDisplayName(const std::string& name)
            {
                const auto isNotSpace = [](unsigned char character) {
                    return !std::isspace(character);
                };

                auto begin = std::find_if(name.begin(), name.end(), isNotSpace);
                if (begin == name.end()) {
                    return "Player";
                }

                auto end = std::find_if(name.rbegin(), name.rend(), isNotSpace).base();
                std::string sanitized(begin, end);
                if (sanitized.size() > kMaxSessionDisplayNameLength) {
                    sanitized.resize(kMaxSessionDisplayNameLength);
                }

                return sanitized.empty() ? "Player" : sanitized;
            }

        }

        OnlineSystem& OnlineSystem::GetInstance()
        {
            static OnlineSystem instance;
            return instance;
        }

        OnlineSystem::~OnlineSystem()
        {
            Shutdown();
        }

        bool OnlineSystem::Initialize(const OnlineConfig& config)
        {
            Shutdown();

            enabled = config.enabled;
            failApplicationOnError = config.failApplicationOnError;
            defaultLobbyBackend = config.backendType;
            defaultLoginOptions.type = config.loginType;
            defaultLoginOptions.displayName = config.loginDisplayName;
            lastError.clear();

            if (!enabled) {
                state = OnlineState::Disabled;
                RTB_INFO("OnlineSystem: disabled.");
                return true;
            }

            RTB_INFO(
                std::string("OnlineSystem: initializing LAN and Relay stacks (default lobby backend: ") +
                ToString(config.backendType) + ").");

            backend = std::make_unique<CompositeOnlineBackend>();

            if (!backend->Initialize(config)) {
                state = OnlineState::Error;
                const char* backendError = backend->GetLastError();
                lastError = backendError && backendError[0] != '\0'
                    ? backendError
                    : "Online backend initialization failed.";

                RTB_ERROR("OnlineSystem: " + lastError);
                backend.reset();
                return !failApplicationOnError;
            }

            state = OnlineState::Initialized;
            RTB_INFO(std::string("OnlineSystem: initialized with backend ") + backend->GetName() + ".");

            return true;
        }

        void OnlineSystem::Tick(float deltaTime)
        {
            if (state != OnlineState::Initialized || !backend) {
                return;
            }

            backend->Tick(deltaTime);
            OnlineGameplayNet::Pump();
        }

        void OnlineSystem::Shutdown()
        {
            ClearPlayerSessionProfiles();

            if (backend) {
                backend->Shutdown();
                backend.reset();
            }

            enabled = false;
            failApplicationOnError = false;
            defaultLobbyBackend = OnlineBackendType::Lan;
            hasSessionLobbyBackend = false;
            sessionLobbyBackend = OnlineBackendType::Lan;
            defaultLoginOptions = OnlineLoginOptions();
            state = OnlineState::Disabled;
        }

        void OnlineSystem::SetSessionDisplayName(const std::string& name)
        {
            defaultLoginOptions.displayName = SanitizeSessionDisplayName(name);
        }

        const std::string& OnlineSystem::GetSessionDisplayName() const
        {
            return defaultLoginOptions.displayName;
        }

        void OnlineSystem::SetSessionLobbyBackend(OnlineBackendType backend)
        {
            sessionLobbyBackend = backend;
            hasSessionLobbyBackend = true;
        }

        void OnlineSystem::ClearSessionLobbyBackend()
        {
            hasSessionLobbyBackend = false;
        }

        bool OnlineSystem::HasSessionLobbyBackend() const
        {
            return hasSessionLobbyBackend;
        }

        OnlineBackendType OnlineSystem::GetSessionLobbyBackend() const
        {
            return sessionLobbyBackend;
        }

        namespace {

            CompositeOnlineBackend* AsCompositeBackend(IOnlineBackend* onlineBackend)
            {
                return static_cast<CompositeOnlineBackend*>(onlineBackend);
            }

            const CompositeOnlineBackend* AsCompositeBackend(const IOnlineBackend* onlineBackend)
            {
                return static_cast<const CompositeOnlineBackend*>(onlineBackend);
            }

        }

        OnlineBackendType OnlineSystem::GetActiveLobbyBackend() const
        {
            const CompositeOnlineBackend* compositeBackend = AsCompositeBackend(backend.get());
            if (!compositeBackend) {
                return hasSessionLobbyBackend ? sessionLobbyBackend : defaultLobbyBackend;
            }

            const OnlineBackendType resolvedBackend = compositeBackend->GetActiveLobbyBackend();

            const IOnlineLobby* lanLobby = compositeBackend->GetLobby(OnlineBackendType::Lan);
            const IOnlineLobby* relayLobby = compositeBackend->GetLobby(OnlineBackendType::RelayOnline);
            const bool lanInLobby = lanLobby && !lanLobby->GetCurrentLobby().lobbyId.empty();
            const bool relayInLobby = relayLobby && !relayLobby->GetCurrentLobby().lobbyId.empty();

            if (!lanInLobby && !relayInLobby && hasSessionLobbyBackend) {
                return sessionLobbyBackend;
            }

            return resolvedBackend;
        }

        bool OnlineSystem::IsLobbyBackendReady(OnlineBackendType lobbyBackend) const
        {
            const CompositeOnlineBackend* compositeBackend = AsCompositeBackend(backend.get());
            return compositeBackend && compositeBackend->IsLobbyBackendReady(lobbyBackend);
        }

        bool OnlineSystem::IsLanLobbyReady() const
        {
            return IsLobbyBackendReady(OnlineBackendType::Lan);
        }

        bool OnlineSystem::IsRelayLobbyReady() const
        {
            return IsLobbyBackendReady(OnlineBackendType::RelayOnline);
        }

        const char* OnlineSystem::GetActiveBackendName() const
        {
            if (!backend) {
                return "None";
            }

            const CompositeOnlineBackend* compositeBackend = AsCompositeBackend(backend.get());
            if (!compositeBackend) {
                return backend->GetName();
            }

            const OnlineBackendType activeLobbyBackend = compositeBackend->GetActiveLobbyBackend();
            const bool inLobby =
                compositeBackend->GetLobby(activeLobbyBackend) &&
                !compositeBackend->GetLobby(activeLobbyBackend)->GetCurrentLobby().lobbyId.empty();

            if (inLobby) {
                return ToString(activeLobbyBackend);
            }

            return backend->GetName();
        }

        IOnlineIdentity* OnlineSystem::GetIdentity()
        {
            return backend ? backend->GetIdentity() : nullptr;
        }

        const IOnlineIdentity* OnlineSystem::GetIdentity() const
        {
            return backend ? backend->GetIdentity() : nullptr;
        }

        IOnlineLobby* OnlineSystem::GetLobby(OnlineBackendType lobbyBackend)
        {
            CompositeOnlineBackend* compositeBackend = AsCompositeBackend(backend.get());
            return compositeBackend ? compositeBackend->GetLobby(lobbyBackend) : nullptr;
        }

        const IOnlineLobby* OnlineSystem::GetLobby(OnlineBackendType lobbyBackend) const
        {
            return const_cast<OnlineSystem*>(this)->GetLobby(lobbyBackend);
        }

        IOnlineLobby* OnlineSystem::GetLobby()
        {
            return backend ? backend->GetLobby() : nullptr;
        }

        const IOnlineLobby* OnlineSystem::GetLobby() const
        {
            return backend ? backend->GetLobby() : nullptr;
        }

        IOnlineTransport* OnlineSystem::GetTransport()
        {
            return backend ? backend->GetTransport() : nullptr;
        }

        const IOnlineTransport* OnlineSystem::GetTransport() const
        {
            return backend ? backend->GetTransport() : nullptr;
        }

        bool OnlineSystem::IsInLobby() const
        {
            const IOnlineLobby* lobby = GetLobby();
            return lobby && !lobby->GetCurrentLobby().lobbyId.empty();
        }

        bool OnlineSystem::IsLobbyOwner() const
        {
            const IOnlineLobby* lobby = GetLobby();
            return lobby && !lobby->GetCurrentLobby().lobbyId.empty() && lobby->GetCurrentLobby().isOwner;
        }

        OnlineUserId OnlineSystem::GetLocalUserId() const
        {
            const IOnlineIdentity* identity = GetIdentity();
            return identity ? identity->GetLocalUserId() : OnlineUserId();
        }

        std::vector<OnlineUserId> OnlineSystem::GetOrderedLobbyMembers() const
        {
            std::vector<OnlineUserId> members;

            const IOnlineLobby* lobby = GetLobby();
            if (!lobby || lobby->GetCurrentLobby().lobbyId.empty()) {
                return members;
            }

            const OnlineLobbyInfo& lobbyInfo = lobby->GetCurrentLobby();
            if (lobbyInfo.ownerUserId.IsValid()) {
                members.push_back(lobbyInfo.ownerUserId);
            }

            for (const OnlineUserId& member : lobbyInfo.memberUserIds) {
                if (!member.IsValid() || member == lobbyInfo.ownerUserId) {
                    continue;
                }

                members.push_back(member);
            }

            return members;
        }

        std::size_t OnlineSystem::GetLocalPlayerIndex() const
        {
            const OnlineUserId localUserId = GetLocalUserId();
            const std::vector<OnlineUserId> members = GetOrderedLobbyMembers();
            for (std::size_t index = 0; index < members.size(); ++index) {
                if (members[index] == localUserId) {
                    return index;
                }
            }

            return 0;
        }

        std::string OnlineSystem::GetLobbyMemberDisplayName(const OnlineUserId& member) const
        {
            const IOnlineLobby* lobby = GetLobby();
            return lobby ? lobby->GetMemberDisplayName(member) : std::string();
        }

        void OnlineSystem::ClearPlayerSessionProfiles()
        {
            playerSessionProfiles.clear();
        }

        void OnlineSystem::SetPlayerSessionProfile(const OnlinePlayerProfile& profile)
        {
            if (profile.playerSlot < 0 || profile.displayName.empty()) {
                return;
            }

            playerSessionProfiles[profile.playerSlot] = profile;

            PlayerSessionProfileChangedEvent eventData;
            eventData.playerSlot = profile.playerSlot;
            eventData.displayName = profile.displayName;
            eventData.removed = false;
            playerSessionProfileChangedEvent.Invoke(eventData);
        }

        void OnlineSystem::RemovePlayerSessionProfile(int playerSlot)
        {
            if (playerSlot < 0) {
                return;
            }

            playerSessionProfiles.erase(playerSlot);

            PlayerSessionProfileChangedEvent eventData;
            eventData.playerSlot = playerSlot;
            eventData.removed = true;
            playerSessionProfileChangedEvent.Invoke(eventData);
        }

        bool OnlineSystem::HasPlayerSessionProfile(int playerSlot) const
        {
            return playerSlot >= 0 && playerSessionProfiles.find(playerSlot) != playerSessionProfiles.end();
        }

        bool OnlineSystem::TryGetPlayerSessionProfile(int playerSlot, OnlinePlayerProfile& outProfile) const
        {
            if (playerSlot < 0) {
                return false;
            }

            const auto it = playerSessionProfiles.find(playerSlot);
            if (it == playerSessionProfiles.end()) {
                return false;
            }

            outProfile = it->second;
            return true;
        }

        std::string OnlineSystem::GetPlayerDisplayName(int playerSlot) const
        {
            OnlinePlayerProfile profile;
            return TryGetPlayerSessionProfile(playerSlot, profile) ? profile.displayName : std::string();
        }

        std::vector<OnlinePlayerProfile> OnlineSystem::GetPlayerSessionProfiles() const
        {
            std::vector<OnlinePlayerProfile> profiles;
            profiles.reserve(playerSessionProfiles.size());

            for (const auto& entry : playerSessionProfiles) {
                profiles.push_back(entry.second);
            }

            std::sort(profiles.begin(), profiles.end(),
                [](const OnlinePlayerProfile& left, const OnlinePlayerProfile& right) {
                    return left.playerSlot < right.playerSlot;
                });

            return profiles;
        }

        Core::EventSubscription OnlineSystem::SubscribeToPlayerSessionProfileChanged(
            PlayerSessionProfileChangedCallback callback)
        {
            return playerSessionProfileChangedEvent.Subscribe(std::move(callback));
        }

    }
}
