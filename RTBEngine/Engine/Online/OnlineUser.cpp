#include "OnlineUser.h"

namespace RTBEngine {
    namespace Online {

        OnlineUserId::OnlineUserId(OnlineUserIdType type, const std::string& value)
            : type(type), value(value)
        {
        }

        bool OnlineUserId::IsValid() const
        {
            return type != OnlineUserIdType::Invalid && !value.empty();
        }

        std::string OnlineUserId::ToString() const
        {
            if (!IsValid()) {
                return "Invalid";
            }

            return std::string(RTBEngine::Online::ToString(type)) + ":" + value;
        }

        bool OnlineUserId::operator==(const OnlineUserId& other) const
        {
            return type == other.type && value == other.value;
        }

        bool OnlineUserId::operator!=(const OnlineUserId& other) const
        {
            return !(*this == other);
        }

        // Converts provider-neutral user id categories to stable diagnostic strings.
        const char* ToString(OnlineUserIdType type)
        {
            switch (type) {
            case OnlineUserIdType::Invalid:
                return "Invalid";
            case OnlineUserIdType::Local:
                return "Local";
            case OnlineUserIdType::EOSProductUser:
                return "EOSProductUser";
            default:
                return "Unknown";
            }
        }

    }
}
