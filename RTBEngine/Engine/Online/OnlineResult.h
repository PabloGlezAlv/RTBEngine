#pragma once

#include "../RTBEngineAPI.h"

#include <string>

namespace RTBEngine {
    namespace Online {

        enum class OnlineErrorCode {
            None,
            Disabled,
            NotImplemented,
            InvalidState,
            InvalidConfig,
            BackendError
        };

#pragma warning(push)
#pragma warning(disable: 4251)
        struct RTB_API OnlineResult {
            bool success = false;
            OnlineErrorCode errorCode = OnlineErrorCode::None;
            std::string message;

            static OnlineResult Success(const std::string& message = "");
            static OnlineResult Failure(OnlineErrorCode errorCode, const std::string& message);
        };
#pragma warning(pop)

        RTB_API const char* ToString(OnlineErrorCode errorCode);

    }
}
