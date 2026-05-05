#include "NullOnlineIdentity.h"

namespace RTBEngine {
    namespace Online {

        // Creates a deterministic local user without contacting any online service.
        OnlineResult NullOnlineIdentity::Login(const OnlineLoginOptions& options)
        {
            displayName = options.displayName.empty() ? "LocalUser" : options.displayName;
            localUserId = OnlineUserId(OnlineUserIdType::Local, displayName);
            lastError.clear();
            status = OnlineLoginStatus::LoggedIn;

            return OnlineResult::Success("Null identity logged in.");
        }

        void NullOnlineIdentity::Logout()
        {
            status = OnlineLoginStatus::NotLoggedIn;
            localUserId = OnlineUserId();
            displayName.clear();
            lastError.clear();
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

    }
}
