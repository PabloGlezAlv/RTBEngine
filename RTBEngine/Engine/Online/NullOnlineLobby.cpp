#include "NullOnlineLobby.h"

#include "IOnlineIdentity.h"

#include <algorithm>
#include <string>
#include <utility>

namespace {

    bool IsOperationInProgress(RTBEngine::Online::OnlineLobbyState state)
    {
        return state == RTBEngine::Online::OnlineLobbyState::Creating ||
            state == RTBEngine::Online::OnlineLobbyState::Searching ||
            state == RTBEngine::Online::OnlineLobbyState::Joining ||
            state == RTBEngine::Online::OnlineLobbyState::Leaving ||
            state == RTBEngine::Online::OnlineLobbyState::Destroying;
    }

}

namespace RTBEngine {
    namespace Online {

        NullOnlineLobby::NullOnlineLobby(IOnlineIdentity* identity)
            : identity(identity)
        {
        }

        OnlineResult NullOnlineLobby::CreateLobby(const OnlineCreateLobbyOptions& options)
        {
            // Null lobby still enforces the same identity precondition as real backends.
            if (!identity || !identity->IsLoggedIn()) {
                lastError = "Cannot create a lobby without a logged-in local identity.";
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
            }

            if (IsOperationInProgress(state)) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
            }

            if (state == OnlineLobbyState::InLobby && !currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Already in a lobby.");
            }

            const std::uint32_t maxMembers = std::max<std::uint32_t>(2, options.maxMembers);

            // Create a deterministic local lobby for editor and offline testing.
            currentLobby = {};
            currentLobby.lobbyId = "null-lobby-" + std::to_string(nextLobbyId++);
            currentLobby.ownerUserId = identity->GetLocalUserId();
            currentLobby.memberUserIds = { currentLobby.ownerUserId };
            currentLobby.currentMembers = 1;
            currentLobby.maxMembers = maxMembers;
            currentLobby.availableSlots = maxMembers > 0 ? maxMembers - 1 : 0;
            currentLobby.isOwner = true;
            searchResults.clear();
            lastError.clear();
            SetState(OnlineLobbyState::InLobby);

            return OnlineResult::Success("Null lobby created.");
        }

        OnlineResult NullOnlineLobby::FindLobbies(const OnlineFindLobbiesOptions& options)
        {
            // Null search keeps editor testing deterministic without contacting a backend.
            if (!identity || !identity->IsLoggedIn()) {
                lastError = "Cannot search lobbies without a logged-in local identity.";
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
            }

            if (IsOperationInProgress(state)) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
            }

            if (state == OnlineLobbyState::InLobby && !currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Leave the current lobby before searching.");
            }

            if (options.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, "Lobby Id is required.");
            }

            searchResults.clear();
            lastError.clear();
            SetState(OnlineLobbyState::Searching);

            OnlineLobbyInfo result;
            result.lobbyId = options.lobbyId;
            result.ownerUserId = OnlineUserId(OnlineUserIdType::Local, "NullHost");
            result.memberUserIds = { result.ownerUserId };
            result.currentMembers = 1;
            result.maxMembers = 6;
            result.availableSlots = 5;
            result.isOwner = false;
            searchResults.push_back(result);

            SetState(OnlineLobbyState::NotInLobby);
            return OnlineResult::Success("Null lobby search completed.");
        }

        OnlineResult NullOnlineLobby::JoinLobby(const OnlineJoinLobbyOptions& options)
        {
            // Null join creates a local membership record that behaves like a remote lobby.
            if (!identity || !identity->IsLoggedIn()) {
                lastError = "Cannot join a lobby without a logged-in local identity.";
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
            }

            if (IsOperationInProgress(state)) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
            }

            if (state == OnlineLobbyState::InLobby && !currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Already in a lobby.");
            }

            if (options.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, "Lobby Id is required.");
            }

            lastError.clear();
            SetState(OnlineLobbyState::Joining);

            currentLobby = {};
            currentLobby.lobbyId = options.lobbyId;
            currentLobby.ownerUserId = OnlineUserId(OnlineUserIdType::Local, "NullHost");
            currentLobby.memberUserIds = { currentLobby.ownerUserId, identity->GetLocalUserId() };
            currentLobby.currentMembers = 2;
            currentLobby.maxMembers = 6;
            currentLobby.availableSlots = 4;
            currentLobby.isOwner = false;

            SetState(OnlineLobbyState::InLobby);
            return OnlineResult::Success("Null lobby joined.");
        }

        OnlineResult NullOnlineLobby::LeaveLobby()
        {
            if (IsOperationInProgress(state)) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
            }

            if (currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "No active lobby to leave.");
            }

            // Members leave their local lobby state without destroying the owner lobby.
            SetState(OnlineLobbyState::Leaving);
            currentLobby = {};
            lastError.clear();
            SetState(OnlineLobbyState::NotInLobby);
            return OnlineResult::Success("Null lobby left.");
        }

        OnlineResult NullOnlineLobby::DestroyLobby()
        {
            if (IsOperationInProgress(state)) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
            }

            if (currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "No active lobby to destroy.");
            }

            if (!currentLobby.isOwner) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Only the lobby owner can destroy the lobby. Use LeaveLobby instead.");
            }

            // Destroy the local-only lobby immediately.
            currentLobby = {};
            searchResults.clear();
            lastError.clear();
            SetState(OnlineLobbyState::NotInLobby);
            return OnlineResult::Success("Null lobby destroyed.");
        }

        OnlineLobbyState NullOnlineLobby::GetState() const
        {
            return state;
        }

        const OnlineLobbyInfo& NullOnlineLobby::GetCurrentLobby() const
        {
            return currentLobby;
        }

        const std::vector<OnlineLobbyInfo>& NullOnlineLobby::GetSearchResults() const
        {
            return searchResults;
        }

        const char* NullOnlineLobby::GetLastError() const
        {
            return lastError.c_str();
        }

        Core::EventSubscription NullOnlineLobby::SubscribeLobbyStatusChanged(Core::Event<OnlineLobbyStatusChangedEvent>::Callback callback)
        {
            return lobbyStatusChanged.Subscribe(std::move(callback));
        }

        void NullOnlineLobby::ClearLobbyStatusChangedListeners()
        {
            lobbyStatusChanged.Clear();
        }

        void NullOnlineLobby::SetState(OnlineLobbyState newState)
        {
            const OnlineLobbyState previousState = state;
            state = newState;

            if (previousState == newState) {
                return;
            }

            lobbyStatusChanged.Invoke({ previousState, state, currentLobby });
        }

    }
}
