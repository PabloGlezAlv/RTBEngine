#pragma once

#include "../RTBEngineAPI.h"

#include <string>

namespace RTBEngine {
    namespace Online {

        // Provider-neutral user id categories.
        enum class OnlineUserIdType {
            Invalid,
            Local,
            EOSProductUser
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
