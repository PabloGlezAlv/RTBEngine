#include "EosOnlineIdentity.h"

namespace RTBEngine {
    namespace Online {

        void EosOnlineIdentity::SetPlatformHandle(void* handle)
        {
            platformHandle = handle;
        }

        void EosOnlineIdentity::ResetPlatformHandle()
        {
            platformHandle = nullptr;
            Logout();
        }

        OnlineResult EosOnlineIdentity::Login(const OnlineLoginOptions&)
        {
            if (!platformHandle) {
                lastError = "EOS platform handle is not available.";
                status = OnlineLoginStatus::Error;
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
            }

            lastError = "EOS identity login is not implemented yet.";
            status = OnlineLoginStatus::Error;
            return OnlineResult::Failure(OnlineErrorCode::NotImplemented, lastError);
        }

        void EosOnlineIdentity::Logout()
        {
            status = OnlineLoginStatus::NotLoggedIn;
            localUserId = OnlineUserId();
            displayName.clear();
            lastError.clear();
        }

        OnlineLoginStatus EosOnlineIdentity::GetLoginStatus() const
        {
            return status;
        }

        const OnlineUserId& EosOnlineIdentity::GetLocalUserId() const
        {
            return localUserId;
        }

        const std::string& EosOnlineIdentity::GetDisplayName() const
        {
            return displayName;
        }

        const char* EosOnlineIdentity::GetLastError() const
        {
            return lastError.c_str();
        }

    }
}
