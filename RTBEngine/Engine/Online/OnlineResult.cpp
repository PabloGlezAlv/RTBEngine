#include "OnlineResult.h"

namespace RTBEngine {
    namespace Online {

        OnlineResult OnlineResult::Success(const std::string& message)
        {
            return { true, OnlineErrorCode::None, message }; // success flag true, no error category
        }

        OnlineResult OnlineResult::Failure(OnlineErrorCode errorCode, const std::string& message)
        {
            return { false, errorCode, message }; // caller inspects errorCode for failure kind
        }

        const char* ToString(OnlineErrorCode errorCode)
        {
            switch (errorCode) {
            case OnlineErrorCode::None:
                return "None";
            case OnlineErrorCode::Disabled:
                return "Disabled"; // online layer turned off
            case OnlineErrorCode::NotImplemented:
                return "NotImplemented";
            case OnlineErrorCode::InvalidState:
                return "InvalidState"; // e.g. join while not logged in
            case OnlineErrorCode::InvalidConfig:
                return "InvalidConfig"; // bad lobby id or peer address
            case OnlineErrorCode::BackendError:
                return "BackendError"; // socket/DNS/timeout failures
            default:
                return "Unknown";
            }
        }

    }
}
