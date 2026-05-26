#include "OnlineTypes.h"

namespace RTBEngine {
    namespace Online {

        const char* ToString(OnlineBackendType backend)
        {
            switch (backend) {
            case OnlineBackendType::Null:
                return "Null";
            case OnlineBackendType::LAN:
                return "LAN";
            default:
                return "Unknown";
            }
        }

        const char* ToString(OnlineState state)
        {
            switch (state) {
            case OnlineState::Disabled:
                return "Disabled";
            case OnlineState::Initialized:
                return "Initialized";
            case OnlineState::Error:
                return "Error";
            default:
                return "Unknown";
            }
        }

    }
}
