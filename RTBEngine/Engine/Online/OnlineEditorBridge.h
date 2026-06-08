#pragma once

#include "../RTBEngineAPI.h"

#include <cstdint>

namespace RTBEngine {
    namespace Online {

        // POD settings passed from RTBEngineEditor into RTBEngine.dll (no std::string across module boundary).
        struct OnlineEditorSettingsPayload {
            bool enabled = true;
            std::int32_t backendType = 0; // Default lobby backend: 0 = LAN, 1 = Relay
            std::uint16_t lanGamePort = 27015;
            std::uint16_t lanDiscoveryPort = 27016;
            char relayMatchmakingUrl[512] = "";
            char defaultHostAddress[256] = "";
            char loginDisplayName[64] = "";
        };

        RTB_API bool InitializeOnlineFromEditorSettings(const OnlineEditorSettingsPayload& payload);

    }
}
