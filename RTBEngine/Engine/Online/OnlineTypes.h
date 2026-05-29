#pragma once

#include "../RTBEngineAPI.h"

namespace RTBEngine {
    namespace Online {

        enum class OnlineBackendType {
            LAN
        };

        enum class OnlineState {
            Disabled,
            Initialized,
            Error
        };

        RTB_API const char* ToString(OnlineBackendType backend);
        RTB_API const char* ToString(OnlineState state);

    }
}
