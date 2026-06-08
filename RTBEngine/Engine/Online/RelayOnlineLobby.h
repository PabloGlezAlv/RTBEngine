#pragma once

#include "IOnlineLobby.h"

#include <chrono>
#include <string>
#include <unordered_map>

namespace RTBEngine {
    namespace Online {

        class IOnlineIdentity;

#pragma warning(push)
#pragma warning(disable: 4251)
        struct RTB_API RelaySessionInfo {
            std::string sessionToken;
            std::string localMemberId;
            std::string relayHost;
            std::uint16_t relayPort = 0;
        };

        class RTB_API RelayOnlineLobby final : public IOnlineLobby {
        public:
            explicit RelayOnlineLobby(IOnlineIdentity* identity);

            bool Configure(const std::string& matchmakingBaseUrl, std::string& outError);
            void Tick(float deltaTime);

            OnlineResult CreateLobby(const OnlineCreateLobbyOptions& options) override;
            OnlineResult FindLobbies(const OnlineFindLobbiesOptions& options) override;
            OnlineResult JoinLobby(const OnlineJoinLobbyOptions& options) override;
            OnlineResult LeaveLobby() override;
            OnlineResult DestroyLobby() override;
            OnlineLobbyState GetState() const override;
            const OnlineLobbyInfo& GetCurrentLobby() const override;
            const std::vector<OnlineLobbyInfo>& GetSearchResults() const override;
            const char* GetLastError() const override;
            Core::EventSubscription SubscribeLobbyStatusChanged(Core::Event<OnlineLobbyStatusChangedEvent>::Callback callback) override;
            void ClearLobbyStatusChangedListeners() override;
            Core::EventSubscription SubscribeMemberJoined(Core::Event<OnlineLobbyMemberJoinedEvent>::Callback callback) override;
            void ClearMemberJoinedListeners() override;
            std::string GetMemberDisplayName(const OnlineUserId& member) const override;

            const RelaySessionInfo& GetRelaySessionInfo() const { return relaySessionInfo; }

        private:
            void SetState(OnlineLobbyState newState);
            std::string NormalizeLobbyCode(std::string lobbyCode) const;
            OnlineResult FetchLobbyInfo(const std::string& lobbyCode, OnlineLobbyInfo& outLobby, std::string& outBody);
            OnlineResult ApplyLobbyResponseBody(
                const std::string& responseBody,
                bool isOwner,
                bool joinedLobby);
            void ApplySearchLobbyInfo(const std::string& responseBody, OnlineLobbyInfo& outLobby);
            void ApplyMemberUpdatesFromPoll(const std::string& responseBody);
            void PollLobbyUpdates();
            void ClearRelaySession();

            IOnlineIdentity* identity = nullptr;
            std::string matchmakingBaseUrl;
            OnlineLobbyState state = OnlineLobbyState::NotInLobby;
            OnlineLobbyInfo currentLobby;
            std::vector<OnlineLobbyInfo> searchResults;
            RelaySessionInfo relaySessionInfo;
            std::string lastError;
            Core::Event<OnlineLobbyStatusChangedEvent> lobbyStatusChanged;
            Core::Event<OnlineLobbyMemberJoinedEvent> memberJoined;
            std::unordered_map<std::string, std::string> memberDisplayNames;
            std::chrono::steady_clock::time_point lastPollTime{};
        };
#pragma warning(pop)

    }
}
