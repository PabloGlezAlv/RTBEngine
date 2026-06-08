#include "RelayOnlineLobby.h"

#include "IOnlineIdentity.h"
#include "OnlineHttpClient.h"
#include "OnlineJson.h"

#include <algorithm>
#include <cctype>

namespace RTBEngine {
    namespace Online {

        namespace {

            bool IsOperationInProgress(OnlineLobbyState lobbyState)
            {
                return lobbyState == OnlineLobbyState::Creating ||
                    lobbyState == OnlineLobbyState::Searching ||
                    lobbyState == OnlineLobbyState::Joining ||
                    lobbyState == OnlineLobbyState::Leaving ||
                    lobbyState == OnlineLobbyState::Destroying;
            }

            OnlineUserId MakeMemberUserId(
                const std::string& memberIdHex,
                const std::string& localMemberIdHex,
                const OnlineUserId& localUserId)
            {
                if (memberIdHex == localMemberIdHex) {
                    return localUserId;
                }

                return OnlineUserId(OnlineUserIdType::RelayPeer, memberIdHex);
            }

        }

        RelayOnlineLobby::RelayOnlineLobby(IOnlineIdentity* lobbyIdentity)
            : identity(lobbyIdentity)
        {
        }

        bool RelayOnlineLobby::Configure(const std::string& configuredMatchmakingBaseUrl, std::string& outError)
        {
            matchmakingBaseUrl = NormalizeHttpBaseUrl(configuredMatchmakingBaseUrl);
            if (matchmakingBaseUrl.empty()) {
                outError = "Relay matchmaking base URL is empty.";
                return false;
            }

            outError.clear();
            return true;
        }

        void RelayOnlineLobby::Tick(float deltaTime)
        {
            (void)deltaTime;

            if (state != OnlineLobbyState::InLobby || currentLobby.lobbyId.empty()) {
                return;
            }

            const auto now = std::chrono::steady_clock::now();
            if (lastPollTime.time_since_epoch().count() != 0 &&
                now - lastPollTime < std::chrono::milliseconds(1000)) {
                return;
            }

            lastPollTime = now;
            PollLobbyUpdates();
        }

        OnlineResult RelayOnlineLobby::CreateLobby(const OnlineCreateLobbyOptions& options)
        {
            if (!identity || !identity->IsLoggedIn()) {
                lastError = "Cannot create a lobby without a logged-in local identity.";
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
            }

            if (IsOperationInProgress(state)) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
            }

            if (!currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Already in a lobby.");
            }

            if (matchmakingBaseUrl.empty()) {
                lastError = "Relay matchmaking base URL is not configured.";
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, lastError);
            }

            SetState(OnlineLobbyState::Creating);

            const std::uint32_t maxMembers = std::max<std::uint32_t>(2, options.maxMembers);
            const std::string requestBody =
                std::string("{\"displayName\":\"") +
                EscapeJsonString(identity->GetDisplayName()) +
                "\",\"maxMembers\":" + std::to_string(maxMembers) + "}";

            const OnlineHttpResponse response =
                OnlineHttpClient::PostJson(matchmakingBaseUrl + "/lobbies", requestBody);
            if (!response.success) {
                lastError = response.error.empty() ? ExtractJsonErrorMessage(response.body) : response.error;
                if (lastError.empty()) {
                    lastError = "Failed to create relay lobby.";
                }

                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
            }

            searchResults.clear();
            const OnlineResult applyResult = ApplyLobbyResponseBody(response.body, true, true);
            if (!applyResult.success) {
                SetState(OnlineLobbyState::Error);
                return applyResult;
            }

            SetState(OnlineLobbyState::InLobby);
            return OnlineResult::Success("Relay lobby created. Share lobby code: " + currentLobby.lobbyId + ".");
        }

        OnlineResult RelayOnlineLobby::FindLobbies(const OnlineFindLobbiesOptions& options)
        {
            if (!identity || !identity->IsLoggedIn()) {
                lastError = "Cannot search lobbies without a logged-in local identity.";
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
            }

            if (options.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, "Lobby Id is required.");
            }

            if (matchmakingBaseUrl.empty()) {
                lastError = "Relay matchmaking base URL is not configured.";
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, lastError);
            }

            SetState(OnlineLobbyState::Searching);
            searchResults.clear();
            lastError.clear();

            OnlineLobbyInfo foundLobby;
            std::string responseBody;
            const OnlineResult fetchResult = FetchLobbyInfo(options.lobbyId, foundLobby, responseBody);
            if (!fetchResult.success) {
                SetState(OnlineLobbyState::NotInLobby);
                return fetchResult;
            }

            searchResults.push_back(std::move(foundLobby));
            SetState(OnlineLobbyState::NotInLobby);
            return OnlineResult::Success("Relay lobby search completed.");
        }

        OnlineResult RelayOnlineLobby::JoinLobby(const OnlineJoinLobbyOptions& options)
        {
            if (!identity || !identity->IsLoggedIn()) {
                lastError = "Cannot join a lobby without a logged-in local identity.";
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
            }

            if (options.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, "Lobby Id is required.");
            }

            if (!currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Already in a lobby.");
            }

            if (matchmakingBaseUrl.empty()) {
                lastError = "Relay matchmaking base URL is not configured.";
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, lastError);
            }

            SetState(OnlineLobbyState::Joining);

            const std::string lobbyCode = NormalizeLobbyCode(options.lobbyId);
            const std::string requestBody =
                std::string("{\"displayName\":\"") + EscapeJsonString(identity->GetDisplayName()) + "\"}";

            const OnlineHttpResponse response = OnlineHttpClient::PostJson(
                matchmakingBaseUrl + "/lobbies/" + lobbyCode + "/join",
                requestBody);
            if (!response.success) {
                lastError = response.error.empty() ? ExtractJsonErrorMessage(response.body) : response.error;
                if (lastError.empty()) {
                    lastError = "Failed to join relay lobby.";
                }

                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
            }

            searchResults.clear();
            const OnlineResult applyResult = ApplyLobbyResponseBody(response.body, false, true);
            if (!applyResult.success) {
                SetState(OnlineLobbyState::Error);
                return applyResult;
            }

            SetState(OnlineLobbyState::InLobby);
            return OnlineResult::Success("Relay lobby joined.");
        }

        OnlineResult RelayOnlineLobby::LeaveLobby()
        {
            if (currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "No active lobby to leave.");
            }

            currentLobby = {};
            memberDisplayNames.clear();
            searchResults.clear();
            ClearRelaySession();
            lastError.clear();
            SetState(OnlineLobbyState::NotInLobby);
            return OnlineResult::Success("Relay lobby left.");
        }

        OnlineResult RelayOnlineLobby::DestroyLobby()
        {
            if (currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "No active lobby to destroy.");
            }

            if (!currentLobby.isOwner) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Only the lobby owner can destroy the lobby.");
            }

            currentLobby = {};
            memberDisplayNames.clear();
            searchResults.clear();
            ClearRelaySession();
            lastError.clear();
            SetState(OnlineLobbyState::NotInLobby);
            return OnlineResult::Success("Relay lobby destroyed.");
        }

        OnlineLobbyState RelayOnlineLobby::GetState() const
        {
            return state;
        }

        const OnlineLobbyInfo& RelayOnlineLobby::GetCurrentLobby() const
        {
            return currentLobby;
        }

        const std::vector<OnlineLobbyInfo>& RelayOnlineLobby::GetSearchResults() const
        {
            return searchResults;
        }

        const char* RelayOnlineLobby::GetLastError() const
        {
            return lastError.c_str();
        }

        Core::EventSubscription RelayOnlineLobby::SubscribeLobbyStatusChanged(
            Core::Event<OnlineLobbyStatusChangedEvent>::Callback callback)
        {
            return lobbyStatusChanged.Subscribe(std::move(callback));
        }

        void RelayOnlineLobby::ClearLobbyStatusChangedListeners()
        {
            lobbyStatusChanged.Clear();
        }

        Core::EventSubscription RelayOnlineLobby::SubscribeMemberJoined(
            Core::Event<OnlineLobbyMemberJoinedEvent>::Callback callback)
        {
            return memberJoined.Subscribe(std::move(callback));
        }

        void RelayOnlineLobby::ClearMemberJoinedListeners()
        {
            memberJoined.Clear();
        }

        std::string RelayOnlineLobby::GetMemberDisplayName(const OnlineUserId& member) const
        {
            if (!member.IsValid()) {
                return {};
            }

            const auto it = memberDisplayNames.find(member.ToString());
            return it != memberDisplayNames.end() ? it->second : std::string();
        }

        void RelayOnlineLobby::SetState(OnlineLobbyState newState)
        {
            const OnlineLobbyState previousState = state;
            state = newState;

            if (previousState == newState) {
                return;
            }

            lobbyStatusChanged.Invoke({ previousState, state, currentLobby });
        }

        std::string RelayOnlineLobby::NormalizeLobbyCode(std::string lobbyCode) const
        {
            std::transform(lobbyCode.begin(), lobbyCode.end(), lobbyCode.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::toupper(character));
                });
            return lobbyCode;
        }

        OnlineResult RelayOnlineLobby::FetchLobbyInfo(
            const std::string& lobbyCode,
            OnlineLobbyInfo& outLobby,
            std::string& outBody)
        {
            const std::string normalizedCode = NormalizeLobbyCode(lobbyCode);
            const OnlineHttpResponse response =
                OnlineHttpClient::Get(matchmakingBaseUrl + "/lobbies/" + normalizedCode);
            if (!response.success) {
                lastError = response.error.empty() ? ExtractJsonErrorMessage(response.body) : response.error;
                if (lastError.empty()) {
                    lastError = "Relay lobby not found.";
                }

                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
            }

            outBody = response.body;
            ApplySearchLobbyInfo(response.body, outLobby);
            return OnlineResult::Success();
        }

        OnlineResult RelayOnlineLobby::ApplyLobbyResponseBody(
            const std::string& responseBody,
            bool isOwner,
            bool joinedLobby)
        {
            const std::string lobbyId = ExtractJsonStringField(responseBody, "lobbyId");
            const std::string sessionToken = ExtractJsonStringField(responseBody, "sessionToken");
            const std::string localMemberId = ExtractJsonStringField(responseBody, "memberId");
            if (lobbyId.empty() || sessionToken.empty() || localMemberId.empty()) {
                lastError = "Relay lobby response is missing lobbyId, sessionToken, or memberId.";
                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
            }

            const std::vector<OnlineJsonMember> members = ExtractJsonMembers(responseBody);
            if (members.empty()) {
                lastError = "Relay lobby response is missing members.";
                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
            }

            const OnlineUserId localUserId = identity->GetLocalUserId();
            currentLobby = {};
            currentLobby.lobbyId = lobbyId;
            currentLobby.isOwner = ExtractJsonBoolField(responseBody, "isOwner", isOwner);
            currentLobby.currentMembers = static_cast<std::uint32_t>(members.size());
            currentLobby.maxMembers = static_cast<std::uint32_t>(std::max(
                static_cast<int>(currentLobby.currentMembers),
                ExtractJsonIntField(responseBody, "maxMembers", 6)));
            currentLobby.availableSlots = static_cast<std::uint32_t>(std::max(
                0,
                ExtractJsonIntField(
                    responseBody,
                    "availableSlots",
                    static_cast<int>(currentLobby.maxMembers - currentLobby.currentMembers))));

            memberDisplayNames.clear();
            for (const OnlineJsonMember& member : members) {
                const OnlineUserId memberUserId =
                    MakeMemberUserId(member.memberId, localMemberId, localUserId);
                if (member.isOwner) {
                    currentLobby.ownerUserId = memberUserId;
                }

                currentLobby.memberUserIds.push_back(memberUserId);
                memberDisplayNames[memberUserId.ToString()] = member.displayName;
            }

            if (!currentLobby.ownerUserId.IsValid()) {
                lastError = "Relay lobby response is missing lobby owner.";
                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
            }

            relaySessionInfo.sessionToken = sessionToken;
            relaySessionInfo.localMemberId = localMemberId;

            const std::size_t relayObjectBegin = responseBody.find("\"relay\":{");
            if (relayObjectBegin != std::string::npos) {
                const std::string relayObject = responseBody.substr(relayObjectBegin);
                relaySessionInfo.relayHost = ExtractJsonStringField(relayObject, "host");
                relaySessionInfo.relayPort = static_cast<std::uint16_t>(
                    ExtractJsonIntField(relayObject, "port", 27100));
            }

            if (relaySessionInfo.relayHost.empty()) {
                lastError = "Relay lobby response is missing relay endpoint.";
                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
            }

            (void)joinedLobby;
            lastError.clear();
            return OnlineResult::Success();
        }

        void RelayOnlineLobby::ApplySearchLobbyInfo(const std::string& responseBody, OnlineLobbyInfo& outLobby)
        {
            outLobby = {};
            outLobby.lobbyId = ExtractJsonStringField(responseBody, "lobbyId");
            outLobby.maxMembers = static_cast<std::uint32_t>(
                std::max(2, ExtractJsonIntField(responseBody, "maxMembers", 6)));
            outLobby.currentMembers = static_cast<std::uint32_t>(
                std::max(0, ExtractJsonIntField(responseBody, "currentMembers", 0)));
            outLobby.availableSlots = static_cast<std::uint32_t>(
                std::max(0, ExtractJsonIntField(responseBody, "availableSlots", 0)));
            outLobby.isOwner = false;

            const std::vector<OnlineJsonMember> members = ExtractJsonMembers(responseBody);
            for (const OnlineJsonMember& member : members) {
                const OnlineUserId memberUserId(OnlineUserIdType::RelayPeer, member.memberId);
                if (member.isOwner) {
                    outLobby.ownerUserId = memberUserId;
                }

                outLobby.memberUserIds.push_back(memberUserId);
            }
        }

        void RelayOnlineLobby::ClearRelaySession()
        {
            relaySessionInfo = {};
            lastPollTime = {};
        }

        void RelayOnlineLobby::PollLobbyUpdates()
        {
            if (matchmakingBaseUrl.empty() || currentLobby.lobbyId.empty()) {
                return;
            }

            OnlineLobbyInfo fetchedLobby;
            std::string responseBody;
            const OnlineResult fetchResult = FetchLobbyInfo(currentLobby.lobbyId, fetchedLobby, responseBody);
            if (!fetchResult.success) {
                return;
            }

            ApplyMemberUpdatesFromPoll(responseBody);
        }

        void RelayOnlineLobby::ApplyMemberUpdatesFromPoll(const std::string& responseBody)
        {
            const std::vector<OnlineJsonMember> members = ExtractJsonMembers(responseBody);
            if (members.empty()) {
                return;
            }

            const OnlineUserId localUserId = identity ? identity->GetLocalUserId() : OnlineUserId();
            const std::string& localMemberId = relaySessionInfo.localMemberId;
            const std::uint32_t previousCount = currentLobby.currentMembers;

            currentLobby.currentMembers = static_cast<std::uint32_t>(members.size());
            currentLobby.maxMembers = static_cast<std::uint32_t>(std::max(
                static_cast<int>(currentLobby.currentMembers),
                ExtractJsonIntField(responseBody, "maxMembers", static_cast<int>(currentLobby.maxMembers))));
            currentLobby.availableSlots = static_cast<std::uint32_t>(std::max(
                0,
                ExtractJsonIntField(
                    responseBody,
                    "availableSlots",
                    static_cast<int>(currentLobby.maxMembers - currentLobby.currentMembers))));

            std::vector<OnlineUserId> updatedMemberIds;
            updatedMemberIds.reserve(members.size());

            for (const OnlineJsonMember& member : members) {
                const OnlineUserId memberUserId =
                    MakeMemberUserId(member.memberId, localMemberId, localUserId);
                updatedMemberIds.push_back(memberUserId);

                const bool alreadyKnown = std::find(
                    currentLobby.memberUserIds.begin(),
                    currentLobby.memberUserIds.end(),
                    memberUserId) != currentLobby.memberUserIds.end();

                memberDisplayNames[memberUserId.ToString()] = member.displayName;

                if (member.isOwner) {
                    currentLobby.ownerUserId = memberUserId;
                }

                if (!alreadyKnown && memberUserId != localUserId) {
                    const std::string joinedName = member.displayName.empty()
                        ? member.memberId
                        : member.displayName;
                    memberJoined.Invoke({ joinedName });
                }
            }

            currentLobby.memberUserIds = std::move(updatedMemberIds);

            if (previousCount != currentLobby.currentMembers) {
                lobbyStatusChanged.Invoke({ state, state, currentLobby });
            }
        }

    }
}
