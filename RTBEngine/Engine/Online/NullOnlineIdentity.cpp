#include "NullOnlineIdentity.h"

#include <utility>

namespace RTBEngine {
    namespace Online {

        // Creates a deterministic local user without contacting any online service.
        OnlineResult NullOnlineIdentity::Login(const OnlineLoginOptions& options)
        {
            // Use the requested name when provided, otherwise create a predictable local user.
            displayName = options.displayName.empty() ? "LocalUser" : options.displayName;
            localUserId = OnlineUserId(OnlineUserIdType::Local, displayName);
            lastError.clear();
            SetStatus(OnlineLoginStatus::LoggedIn);

            return OnlineResult::Success("Null identity logged in.");
        }

        void NullOnlineIdentity::Logout()
        {
            // Reset local-only identity state.
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

        OnlineLoginStatus NullOnlineIdentity::GetLoginStatus() const
        {
            return status;
        }

        const OnlineUserId& NullOnlineIdentity::GetLocalUserId() const
        {
            return localUserId;
        }

        const std::string& NullOnlineIdentity::GetDisplayName() const
        {
            return displayName;
        }

        const char* NullOnlineIdentity::GetLastError() const
        {
            return lastError.c_str();
        }

        Core::EventSubscription NullOnlineIdentity::SubscribeLoginStatusChanged(Core::Event<OnlineLoginStatusChangedEvent>::Callback callback)
        {
            // Allow external systems to react to local identity state changes.
            return loginStatusChanged.Subscribe(std::move(callback));
        }

        void NullOnlineIdentity::ClearLoginStatusChangedListeners()
        {
            // Drop all identity listeners, usually during tool shutdown or scene reset.
            loginStatusChanged.Clear();
        }

        void NullOnlineIdentity::SetStatus(OnlineLoginStatus newStatus)
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
