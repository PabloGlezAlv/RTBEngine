#pragma once

#include "../RTBEngineAPI.h"
#include "OnlineTypes.h"

#include <cstdint>
#include <string>

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        // Runtime options used by ApplicationConfig to decide if the online layer
        // should start and which backend should own the SDK integration.
        struct RTB_API OnlineConfig {
            bool enabled = false;
            bool failApplicationOnError = false;
            OnlineBackendType backend = OnlineBackendType::EOS;

            // Public product metadata.
            std::string productName = "RTBEngine";
            std::string productVersion = "0.1.0";

            // EOS portal identifiers and service credentials.
            std::string productId;
            std::string sandboxId;
            std::string deploymentId;
            std::string clientId;
            std::string clientSecret;

            bool isServer = false;
            bool loadingInEditor = false;
            bool disableOverlay = true;
            std::string cacheDirectory;
            std::uint32_t tickBudgetMilliseconds = 0;
        };
#pragma warning(pop)

    }
}
