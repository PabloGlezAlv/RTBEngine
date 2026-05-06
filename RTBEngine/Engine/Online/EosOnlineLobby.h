#pragma once

#include "IOnlineLobby.h"

#include <memory>

namespace RTBEngine {
    namespace Online {

        class IOnlineIdentity;

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API EosOnlineLobby final : public IOnlineLobby {
        public:
            explicit EosOnlineLobby(IOnlineIdentity* identity);
            ~EosOnlineLobby() override;

            void SetPlatformHandle(void* handle);
            void ResetPlatformHandle();

            OnlineResult CreateLobby(const OnlineCreateLobbyOptions& options) override;
            OnlineResult DestroyLobby() override;
            OnlineLobbyState GetState() const override;
            const OnlineLobbyInfo& GetCurrentLobby() const override;
            const char* GetLastError() const override;
            Core::EventSubscription SubscribeLobbyStatusChanged(Core::Event<OnlineLobbyStatusChangedEvent>::Callback callback) override;
            void ClearLobbyStatusChangedListeners() override;

        private:
            class Impl;
            std::unique_ptr<Impl> impl;
        };
#pragma warning(pop)

    }
}
