#pragma once

#include "../RTBEngineAPI.h"
#include "../Core/Event.h"
#include "OnlineResult.h"
#include "OnlineUser.h"

#include <cstdint>
#include <string>

namespace RTBEngine {
    namespace Online {

        enum class OnlineLobbyState {
            NotInLobby,
            Creating,
            InLobby,
            Destroying,
            Error
        };

#pragma warning(push)
#pragma warning(disable: 4251)
        struct RTB_API OnlineLobbyInfo {
            std::string lobbyId;
            OnlineUserId ownerUserId;
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

        struct RTB_API OnlineLobbyStatusChangedEvent {
            OnlineLobbyState previousState = OnlineLobbyState::NotInLobby;
            OnlineLobbyState currentState = OnlineLobbyState::NotInLobby;
            OnlineLobbyInfo lobby;
        };

        class RTB_API IOnlineLobby {
        public:
            virtual ~IOnlineLobby() = default;

            virtual OnlineResult CreateLobby(const OnlineCreateLobbyOptions& options) = 0;
            virtual OnlineResult DestroyLobby() = 0;
            virtual OnlineLobbyState GetState() const = 0;
            virtual const OnlineLobbyInfo& GetCurrentLobby() const = 0;
            virtual const char* GetLastError() const = 0;
            virtual Core::EventSubscription SubscribeLobbyStatusChanged(Core::Event<OnlineLobbyStatusChangedEvent>::Callback callback) = 0;
            virtual void ClearLobbyStatusChangedListeners() = 0;

            bool IsInLobby() const { return GetState() == OnlineLobbyState::InLobby; }
        };
#pragma warning(pop)

        RTB_API const char* ToString(OnlineLobbyState state);

    }
}
