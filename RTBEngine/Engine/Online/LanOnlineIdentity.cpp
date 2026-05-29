#include "LanOnlineIdentity.h"

#include <utility>

namespace RTBEngine {
    namespace Online {

        OnlineResult LanOnlineIdentity::Login(const OnlineLoginOptions& options)
        {
            displayName = options.displayName.empty() ? "LocalUser" : options.displayName;
            localUserId = OnlineUserId(OnlineUserIdType::Local, displayName);
            lastError.clear();
            SetStatus(OnlineLoginStatus::LoggedIn);

            return OnlineResult::Success("Local identity logged in.");
        }

        void LanOnlineIdentity::Logout()
        {
            const bool wasLoggedIn = status != OnlineLoginStatus::NotLoggedIn;
            localUserId = OnlineUserId();
            displayName.clear();
            lastError.clear();

            if (wasLoggedIn) {
                SetStatus(OnlineLoginStatus::NotLoggedIn);
            } else {
                status = OnlineLoginStatus::NotLoggedIn;
            }
        }

        OnlineLoginStatus LanOnlineIdentity::GetLoginStatus() const
        {
            return status;
        }

        const OnlineUserId& LanOnlineIdentity::GetLocalUserId() const
        {
            return localUserId;
        }

        const std::string& LanOnlineIdentity::GetDisplayName() const
        {
            return displayName;
        }

        const char* LanOnlineIdentity::GetLastError() const
        {
            return lastError.c_str();
        }

        Core::EventSubscription LanOnlineIdentity::SubscribeLoginStatusChanged(Core::Event<OnlineLoginStatusChangedEvent>::Callback callback)
        {
            return loginStatusChanged.Subscribe(std::move(callback));
        }

        void LanOnlineIdentity::ClearLoginStatusChangedListeners()
        {
            loginStatusChanged.Clear();
        }

        void LanOnlineIdentity::SetStatus(OnlineLoginStatus newStatus)
        {
            const OnlineLoginStatus previousStatus = status;
            status = newStatus;

            if (previousStatus == newStatus) {
                return;
            }

            loginStatusChanged.Invoke({ previousStatus, status, localUserId });
        }

    }
}
