#include "IOnlineIdentity.h"

namespace RTBEngine {
    namespace Online {

        const char* ToString(OnlineLoginType type)
        {
            switch (type) {
            case OnlineLoginType::DeviceId:
                return "DeviceId";           // local device id login (current default)
            case OnlineLoginType::DeveloperAuth:
                return "DeveloperAuth";     // reserved for dev credential flow
            case OnlineLoginType::AccountPortal:
                return "AccountPortal";     // reserved for account portal flow
            default:
                return "Unknown";
            }
        }

        const char* ToString(OnlineLoginStatus status)
        {
            switch (status) {
            case OnlineLoginStatus::NotLoggedIn:
                return "NotLoggedIn";
            case OnlineLoginStatus::LoggingIn:
                return "LoggingIn"; // async login in progress (future backends)
            case OnlineLoginStatus::LoggedIn:
                return "LoggedIn";
            case OnlineLoginStatus::Error:
                return "Error";
            default:
                return "Unknown";
            }
        }

    }
}
