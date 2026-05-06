#pragma once

#include "IOnlineLobby.h"

namespace RTBEngine {
    namespace Online {

        class IOnlineIdentity;

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API NullOnlineLobby final : public IOnlineLobby {
        public:
            explicit NullOnlineLobby(IOnlineIdentity* identity);

            OnlineResult CreateLobby(const OnlineCreateLobbyOptions& options) override;
            OnlineResult DestroyLobby() override;
            OnlineLobbyState GetState() const override;
            const OnlineLobbyInfo& GetCurrentLobby() const override;
            const char* GetLastError() const override;
            Core::EventSubscription SubscribeLobbyStatusChanged(Core::Event<OnlineLobbyStatusChangedEvent>::Callback callback) override;
            void ClearLobbyStatusChangedListeners() override;

        private:
            void SetState(OnlineLobbyState newState);

            IOnlineIdentity* identity = nullptr;
            OnlineLobbyState state = OnlineLobbyState::NotInLobby;
            OnlineLobbyInfo currentLobby;
            std::string lastError;
            std::uint32_t nextLobbyId = 1;
            Core::Event<OnlineLobbyStatusChangedEvent> lobbyStatusChanged;
        };
#pragma warning(pop)

    }
}
