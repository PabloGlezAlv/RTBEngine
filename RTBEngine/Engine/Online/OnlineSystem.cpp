#include "OnlineSystem.h"

#include "LanOnlineBackend.h"
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
            defaultLoginOptions.type = config.loginType;
            defaultLoginOptions.displayName = config.loginDisplayName;
            lastError.clear();

            if (!enabled) {
                state = OnlineState::Disabled;
                RTB_INFO("OnlineSystem: disabled.");
                return true;
            }

            backend = std::make_unique<LanOnlineBackend>();

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
            if (backend) {
                backend->Shutdown();
                backend.reset();
            }

            enabled = false;
            failApplicationOnError = false;
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

        IOnlineIdentity* OnlineSystem::GetIdentity()
        {
            return backend ? backend->GetIdentity() : nullptr;
        }

        const IOnlineIdentity* OnlineSystem::GetIdentity() const
        {
            return backend ? backend->GetIdentity() : nullptr;
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

    }
}
