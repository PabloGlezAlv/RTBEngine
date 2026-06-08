#include "OnlineUser.h"

namespace RTBEngine {
    namespace Online {

        OnlineUserId::OnlineUserId(OnlineUserIdType type, const std::string& value)
            : type(type), value(value) // value is id body without "Local:" prefix
        {
        }

        bool OnlineUserId::IsValid() const
        {
            return type != OnlineUserIdType::Invalid && !value.empty(); // Invalid type or empty string = unusable id
        }

        std::string OnlineUserId::ToString() const
        {
            if (!IsValid()) {
                return "Invalid"; // stable log text for default-constructed ids
            }

            return std::string(RTBEngine::Online::ToString(type)) + ":" + value; // e.g. NetworkPeer:192.168.0.2:27015
        }

        bool OnlineUserId::operator==(const OnlineUserId& other) const
        {
            return type == other.type && value == other.value; // both type tag and payload must match
        }

        bool OnlineUserId::operator!=(const OnlineUserId& other) const
        {
            return !(*this == other);
        }

        const char* ToString(OnlineUserIdType type)
        {
            switch (type) {
            case OnlineUserIdType::Invalid:
                return "Invalid";
            case OnlineUserIdType::Local:
                return "Local"; // this machine's logged-in user
            case OnlineUserIdType::RelayPeer:
                return "RelayPeer"; // remote player via relay memberId
            case OnlineUserIdType::NetworkPeer:
                return "NetworkPeer"; // remote UDP endpoint identity
            default:
                return "Unknown";
            }
        }

    }
}
