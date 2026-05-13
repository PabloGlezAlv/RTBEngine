#include "IOnlineIdentity.h"

namespace RTBEngine {
    namespace Online {

        // Converts supported login methods to stable diagnostic strings.
        const char* ToString(OnlineLoginType type)
        {
            switch (type) {
            case OnlineLoginType::DeviceId:
                return "DeviceId";
            case OnlineLoginType::DeveloperAuth:
                return "DeveloperAuth";
            case OnlineLoginType::AccountPortal:
                return "AccountPortal";
            default:
                return "Unknown";
            }
        }

        // Converts identity lifecycle states to stable diagnostic strings.
        const char* ToString(OnlineLoginStatus status)
        {
            switch (status) {
            case OnlineLoginStatus::NotLoggedIn:
                return "NotLoggedIn";
            case OnlineLoginStatus::LoggingIn:
                return "LoggingIn";
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
