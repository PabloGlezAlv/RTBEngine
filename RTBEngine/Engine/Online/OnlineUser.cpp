#include "OnlineUser.h"

namespace RTBEngine {
    namespace Online {

        OnlineUserId::OnlineUserId(OnlineUserIdType type, const std::string& value)
            : type(type), value(value)
        {
        }

        bool OnlineUserId::IsValid() const
        {
            // A user id is valid only when it has both a provider type and a value.
            return type != OnlineUserIdType::Invalid && !value.empty();
        }

        std::string OnlineUserId::ToString() const
        {
            // Keep invalid ids readable in logs.
            if (!IsValid()) {
                return "Invalid";
            }

            // Prefix the value with its provider-neutral id type.
            return std::string(RTBEngine::Online::ToString(type)) + ":" + value;
        }

        bool OnlineUserId::operator==(const OnlineUserId& other) const
        {
            // User ids match only when both the type and provider value match.
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
            case OnlineUserIdType::NetworkPeer:
                return "NetworkPeer";
            default:
                return "Unknown";
            }
        }

    }
}
