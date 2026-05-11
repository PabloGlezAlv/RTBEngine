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
            std::uint32_t maxResults = 10;
        };

        struct RTB_API OnlineJoinLobbyOptions {
            std::string lobbyId;
        };

        struct RTB_API OnlineLobbyStatusChangedEvent {
            OnlineLobbyState previousState = OnlineLobbyState::NotInLobby;
            OnlineLobbyState currentState = OnlineLobbyState::NotInLobby;
            OnlineLobbyInfo lobby;
        };

        class RTB_API IOnlineLobby {
        public:
            virtual ~IOnlineLobby() = default;

            virtual OnlineResult CreateLobby(const OnlineCreateLobbyOptions& options) = 0;
            virtual OnlineResult FindLobbies(const OnlineFindLobbiesOptions& options) = 0;
            virtual OnlineResult JoinLobby(const OnlineJoinLobbyOptions& options) = 0;
            virtual OnlineResult LeaveLobby() = 0;
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
