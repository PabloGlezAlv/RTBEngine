#pragma once

#include "../RTBEngineAPI.h"
#include "IOnlineIdentity.h"
#include "OnlineTypes.h"

#include <cstdint>
#include <string>

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        // Runtime options used by ApplicationConfig to decide if the online layer
        // should start and which backend should own networking.
        struct OnlineConfig {
            bool enabled = false;
            bool failApplicationOnError = false;
            OnlineBackendType backend = OnlineBackendType::LAN;

            // Public product metadata.
            std::string productName = "RTBEngine";
            std::string productVersion = "0.1.0";

            // UDP backend ports. Each local test player should use unique values.
            std::uint16_t lanGamePort = 27015;
            std::uint16_t lanDiscoveryPort = 27016;
            // Default host to contact when joining from game/editor (empty = local broadcast only).
            std::string defaultHostAddress;

            bool isServer = false;
            bool loadingInEditor = false;
            std::string cacheDirectory;
            std::uint32_t tickBudgetMilliseconds = 0;

            // Default local-user login used by runtime scenes/tools that auto-login.
            OnlineLoginType loginType = OnlineLoginType::DeviceId;
            std::string loginDisplayName;
        };
#pragma warning(pop)

    }
}
