#pragma once

#include "IOnlineIdentity.h"

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API LanOnlineIdentity final : public IOnlineIdentity {
        public:
            OnlineResult Login(const OnlineLoginOptions& options) override;
            void Logout() override;
            OnlineLoginStatus GetLoginStatus() const override;
            const OnlineUserId& GetLocalUserId() const override;
            const std::string& GetDisplayName() const override;
            const char* GetLastError() const override;
            Core::EventSubscription SubscribeLoginStatusChanged(Core::Event<OnlineLoginStatusChangedEvent>::Callback callback) override;
            void ClearLoginStatusChangedListeners() override;

        private:
            void SetStatus(OnlineLoginStatus newStatus);

            OnlineLoginStatus status = OnlineLoginStatus::NotLoggedIn;
            OnlineUserId localUserId;
            std::string displayName;
            std::string lastError;
            Core::Event<OnlineLoginStatusChangedEvent> loginStatusChanged;
        };
#pragma warning(pop)

    }
}
