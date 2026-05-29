#pragma once

#include "../RTBEngineAPI.h"
#include "IOnlineIdentity.h"

#include <cstdint>
#include <string>

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        struct OnlineConfig {
            bool enabled = false;
            bool failApplicationOnError = false;

            std::string productName = "RTBEngine";
            std::string productVersion = "0.1.0";

            std::uint16_t lanGamePort = 27015;
            std::uint16_t lanDiscoveryPort = 27016;
            std::string defaultHostAddress;

            bool isServer = false;
            bool loadingInEditor = false;
            std::string cacheDirectory;
            std::uint32_t tickBudgetMilliseconds = 0;

            OnlineLoginType loginType = OnlineLoginType::DeviceId;
            std::string loginDisplayName;
        };
#pragma warning(pop)

    }
}
