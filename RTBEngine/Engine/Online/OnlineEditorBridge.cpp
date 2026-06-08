#include "OnlineEditorBridge.h"

#include "OnlineConfig.h"
#include "OnlineSystem.h"
#include "OnlineTypes.h"
#include "../Core/Logger.h"

namespace RTBEngine {
    namespace Online {

        bool InitializeOnlineFromEditorSettings(const OnlineEditorSettingsPayload& payload)
        {
            OnlineConfig config;
            config.enabled = payload.enabled;
            config.failApplicationOnError = false;
            config.loadingInEditor = true;
            config.lanGamePort = payload.lanGamePort;
            config.lanDiscoveryPort = payload.lanDiscoveryPort;
            config.defaultHostAddress = payload.defaultHostAddress;
            config.relayMatchmakingUrl = payload.relayMatchmakingUrl;
            config.loginDisplayName = payload.loginDisplayName;
            config.loginType = OnlineLoginType::DeviceId;
            config.backendType = payload.backendType == 1
                ? OnlineBackendType::RelayOnline
                : OnlineBackendType::Lan;

            RTB_INFO(
                std::string("OnlineEditorBridge: default lobby backend ") +
                ToString(config.backendType) + " (enabled=" + (config.enabled ? "true" : "false") + ").");

            return OnlineSystem::GetInstance().Initialize(config);
        }

    }
}
