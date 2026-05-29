#pragma once

#include "../RTBEngineAPI.h"
#include "../Core/Event.h"
#include "OnlineResult.h"
#include "OnlineUser.h"

#include <cstdint>
#include <string>
#include <vector>

namespace RTBEngine {
    namespace Online {

        enum class OnlineLobbyState {
            NotInLobby,
            Creating,
            Searching,
            Joining,
            InLobby,
            Leaving,
            Destroying,
            Error
        };

#pragma warning(push)
#pragma warning(disable: 4251)
        struct RTB_API OnlineLobbyInfo {
            std::string lobbyId;
            OnlineUserId ownerUserId;
            std::vector<OnlineUserId> memberUserIds;
            std::uint32_t currentMembers = 0;
            std::uint32_t maxMembers = 0;
            std::uint32_t availableSlots = 0;
            bool isOwner = false;
        };

        struct RTB_API OnlineCreateLobbyOptions {
            std::uint32_t maxMembers = 6;
            std::string bucketId = "RTBEngine";
            bool publicAdvertised = true;
            bool allowInvites = true;
            bool allowJoinById = true;
            bool allowHostMigration = false;
            bool enableRtcRoom = false;
        };

        struct RTB_API OnlineFindLobbiesOptions {
            std::string lobbyId;
            // Empty = UDP broadcast on LAN. Set host IP/DNS for direct Internet lookup.
            std::string hostAddress;
            std::uint32_t maxResults = 10;
        };

        struct RTB_API OnlineJoinLobbyOptions {
            std::string lobbyId;
            // Empty = LAN broadcast search. Set host IP/DNS to join over Internet.
            std::string hostAddress;
        };

        struct RTB_API OnlineLobbyStatusChangedEvent {
            OnlineLobbyState previousState = OnlineLobbyState::NotInLobby;
            OnlineLobbyState currentState = OnlineLobbyState::NotInLobby;
            OnlineLobbyInfo lobby;
        };

        class RTB_API IOnlineLobby {
        public:
            virtual ~IOnlineLobby() = default;

            // Host-only: allocates a lobby id and starts advertising on the discovery port.
            virtual OnlineResult CreateLobby(const OnlineCreateLobbyOptions& options) = 0;
            // Sends RTB_FIND on discovery; fills searchResults with RTB_FOUND replies.
            virtual OnlineResult FindLobbies(const OnlineFindLobbiesOptions& options) = 0;
            // Sends RTB_JOIN; host replies with RTB_JOIN_ACK and updates memberUserIds.
            virtual OnlineResult JoinLobby(const OnlineJoinLobbyOptions& options) = 0;
            virtual OnlineResult LeaveLobby() = 0;
            // Owner-only: tears down the lobby and stops advertising.
            virtual OnlineResult DestroyLobby() = 0;
            virtual OnlineLobbyState GetState() const = 0;
            virtual const OnlineLobbyInfo& GetCurrentLobby() const = 0;
            virtual const std::vector<OnlineLobbyInfo>& GetSearchResults() const = 0;
            virtual const char* GetLastError() const = 0;
            virtual Core::EventSubscription SubscribeLobbyStatusChanged(Core::Event<OnlineLobbyStatusChangedEvent>::Callback callback) = 0;
            virtual void ClearLobbyStatusChangedListeners() = 0;

            bool IsInLobby() const { return GetState() == OnlineLobbyState::InLobby; }
        };
#pragma warning(pop)

        RTB_API const char* ToString(OnlineLobbyState state);

    }
}
