#include "OnlineResult.h"

namespace RTBEngine {
    namespace Online {

        OnlineResult OnlineResult::Success(const std::string& message)
        {
            return { true, OnlineErrorCode::None, message };
        }

        OnlineResult OnlineResult::Failure(OnlineErrorCode errorCode, const std::string& message)
        {
            return { false, errorCode, message };
        }

        // Converts shared error categories to stable diagnostic strings.
        const char* ToString(OnlineErrorCode errorCode)
        {
            switch (errorCode) {
            case OnlineErrorCode::None:
                return "None";
            case OnlineErrorCode::Disabled:
                return "Disabled";
            case OnlineErrorCode::NotImplemented:
                return "NotImplemented";
            case OnlineErrorCode::InvalidState:
                return "InvalidState";
            case OnlineErrorCode::InvalidConfig:
                return "InvalidConfig";
            case OnlineErrorCode::BackendError:
                return "BackendError";
            default:
                return "Unknown";
            }
        }

    }
}
