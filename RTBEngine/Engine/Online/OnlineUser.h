#pragma once

#include "../RTBEngineAPI.h"

#include <string>

namespace RTBEngine {
    namespace Online {

        enum class OnlineUserIdType {
            Invalid,
            Local,        // this machine's logged-in user
            RelayPeer,    // remote player as relay memberId hex
            NetworkPeer   // remote machine as host:port (LAN transport addressing)
        };

#pragma warning(push)
#pragma warning(disable: 4251) 
        class RTB_API OnlineUserId {
        public:
            OnlineUserId() = default;
            OnlineUserId(OnlineUserIdType type, const std::string& value);

            bool IsValid() const;
            OnlineUserIdType GetType() const { return type; }
            const std::string& GetValue() const { return value; }
            // Formats as "Type:value", e.g. "Local:Player1" or "NetworkPeer:192.168.0.2:27015".
            std::string ToString() const;

            bool operator==(const OnlineUserId& other) const;
            bool operator!=(const OnlineUserId& other) const;

        private:
            OnlineUserIdType type = OnlineUserIdType::Invalid;
            std::string value;
        };
#pragma warning(pop)

        RTB_API const char* ToString(OnlineUserIdType type);

    }
}
