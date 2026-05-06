#include "NullOnlineLobby.h"

#include "IOnlineIdentity.h"

#include <algorithm>
#include <string>
#include <utility>

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

            if (state == OnlineLobbyState::Creating || state == OnlineLobbyState::Destroying) {
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
            currentLobby.maxMembers = maxMembers;
            currentLobby.availableSlots = maxMembers > 0 ? maxMembers - 1 : 0;
            currentLobby.isOwner = true;
            lastError.clear();
            SetState(OnlineLobbyState::InLobby);

            return OnlineResult::Success("Null lobby created.");
        }

        OnlineResult NullOnlineLobby::DestroyLobby()
        {
            if (state == OnlineLobbyState::Creating || state == OnlineLobbyState::Destroying) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
            }

            if (currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "No active lobby to destroy.");
            }

            // Destroy the local-only lobby immediately.
            currentLobby = {};
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
