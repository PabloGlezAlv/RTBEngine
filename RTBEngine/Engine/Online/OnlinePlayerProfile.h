#pragma once

#include "../RTBEngineAPI.h"
#include "OnlineUser.h"

#include <string>

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        struct RTB_API OnlinePlayerProfile {
            OnlineUserId userId;
            int playerSlot = -1;
            std::string displayName;
        };
#pragma warning(pop)

    }
}
