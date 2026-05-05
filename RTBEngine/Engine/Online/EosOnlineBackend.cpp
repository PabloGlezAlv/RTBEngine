#include "EosOnlineBackend.h"

#include "OnlineConfig.h"
#include "../Core/Logger.h"

#include <eos_common.h>
#include <eos_init.h>
#include <eos_logging.h>
#include <eos_sdk.h>
#include <eos_types.h>
#include <eos_version.h>

#include <sstream>
#include <string>
#include <vector>

namespace {

    // Local helper utilities kept private to this translation unit.
    bool IsEmpty(const std::string& value)
    {
        return value.empty();
    }

    std::string EosResultToString(EOS_EResult result)
    {
        const char* resultText = EOS_EResult_ToString(result);
        return resultText ? resultText : "EOS_Unknown";
    }

    // Bridges EOS SDK log messages into the engine logger.
    void EOS_CALL OnEosLogMessage(const EOS_LogMessage* message)
    {
        if (!message) {
            return;
        }

        std::string text = "[EOS]";
        if (message->Category && message->Category[0] != '\0') {
            text += "[";
            text += message->Category;
            text += "]";
        }

        text += " ";
        text += message->Message ? message->Message : "";

        if (message->Level <= EOS_ELogLevel::EOS_LOG_Error) {
            RTB_ERROR(text);
        } else if (message->Level == EOS_ELogLevel::EOS_LOG_Warning) {
            RTB_WARN(text);
        } else {
            RTB_INFO(text);
        }
    }

    // Validates the minimum EOS portal fields required to create a platform handle.
    std::string BuildMissingEosConfigMessage(const RTBEngine::Online::OnlineConfig& config)
    {
        std::vector<const char*> missingFields;
        if (IsEmpty(config.productId)) missingFields.push_back("productId");
        if (IsEmpty(config.sandboxId)) missingFields.push_back("sandboxId");
        if (IsEmpty(config.deploymentId)) missingFields.push_back("deploymentId");
        if (IsEmpty(config.clientId)) missingFields.push_back("clientId");
        if (IsEmpty(config.clientSecret)) missingFields.push_back("clientSecret");

        if (missingFields.empty()) {
            return {};
        }

        std::ostringstream stream;
        stream << "Missing EOS configuration fields: ";
        for (size_t i = 0; i < missingFields.size(); ++i) {
            if (i > 0) {
                stream << ", ";
            }

            stream << missingFields[i];
        }

        return stream.str();
    }

}

namespace RTBEngine {
    namespace Online {

        EosOnlineBackend::~EosOnlineBackend()
        {
            Shutdown();
        }

        const char* EosOnlineBackend::GetName() const
        {
            return "EOS";
        }

        // Initializes the global EOS SDK state and creates the platform instance.
        // Future:Auth, Lobby, or P2P...
        bool EosOnlineBackend::Initialize(const OnlineConfig& config)
        {
            if (initialized) {
                return true;
            }

            lastError.clear();

            const std::string missingConfig = BuildMissingEosConfigMessage(config);
            if (!missingConfig.empty()) {
                lastError = missingConfig;
                RTB_ERROR("OnlineSystem: " + lastError);
                return false;
            }

            // EOS_Initialize must run before any other EOS SDK call.
            EOS_InitializeOptions initializeOptions{};
            initializeOptions.ApiVersion = EOS_INITIALIZE_API_LATEST;
            initializeOptions.ProductName = config.productName.empty() ? "RTBEngine" : config.productName.c_str();
            initializeOptions.ProductVersion = config.productVersion.empty() ? "0.1.0" : config.productVersion.c_str();

            const EOS_EResult initializeResult = EOS_Initialize(&initializeOptions);
            if (initializeResult != EOS_EResult::EOS_Success &&
                initializeResult != EOS_EResult::EOS_AlreadyConfigured) {
                lastError = "EOS_Initialize failed: " + EosResultToString(initializeResult);
                RTB_ERROR("OnlineSystem: " + lastError);
                return false;
            }

            eosInitializedByBackend = (initializeResult == EOS_EResult::EOS_Success);

            // Route EOS diagnostics through RTBEngine's logging system.
            EOS_Logging_SetCallback(&OnEosLogMessage);
            EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, EOS_ELogLevel::EOS_LOG_Warning);

            // EOS_Platform_Create returns the main handle used by all future EOS interfaces.
            EOS_Platform_Options platformOptions{};
            platformOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
            platformOptions.ProductId = config.productId.c_str();
            platformOptions.SandboxId = config.sandboxId.c_str();
            platformOptions.DeploymentId = config.deploymentId.c_str();
            platformOptions.ClientCredentials.ClientId = config.clientId.c_str();
            platformOptions.ClientCredentials.ClientSecret = config.clientSecret.c_str();
            platformOptions.bIsServer = config.isServer ? EOS_TRUE : EOS_FALSE;
            platformOptions.TickBudgetInMilliseconds = config.tickBudgetMilliseconds;

            if (config.loadingInEditor) {
                platformOptions.Flags |= EOS_PF_LOADING_IN_EDITOR;
            }

            if (config.disableOverlay) {
                platformOptions.Flags |= EOS_PF_DISABLE_OVERLAY;
                platformOptions.Flags |= EOS_PF_DISABLE_SOCIAL_OVERLAY;
            }

            platformHandle = EOS_Platform_Create(&platformOptions);
            if (!platformHandle) {
                lastError = "EOS_Platform_Create failed.";
                RTB_ERROR("OnlineSystem: " + lastError);
                Shutdown();
                return false;
            }

            identity.SetPlatformHandle(platformHandle);

            initialized = true;
            RTB_INFO(std::string("OnlineSystem: EOS backend initialized. SDK version: ") + EOS_GetVersion());
            return true;
        }

        // EOS processes asynchronous SDK work from this call.
        // Future login, lobby, and P2P callbacks will depend on this running every frame.
        void EosOnlineBackend::Tick(float)
        {
            if (!platformHandle) {
                return;
            }

            EOS_Platform_Tick(static_cast<EOS_HPlatform>(platformHandle));
        }

        // Releases the platform handle before shutting down the global EOS SDK state.
        void EosOnlineBackend::Shutdown()
        {
            identity.ResetPlatformHandle();

            if (platformHandle) {
                EOS_Platform_Release(static_cast<EOS_HPlatform>(platformHandle));
                platformHandle = nullptr;
            }

            if (eosInitializedByBackend) {
                EOS_Logging_SetCallback(nullptr);
                const EOS_EResult shutdownResult = EOS_Shutdown();
                if (shutdownResult != EOS_EResult::EOS_Success &&
                    shutdownResult != EOS_EResult::EOS_NotConfigured) {
                    RTB_WARN("OnlineSystem: EOS_Shutdown returned " + EosResultToString(shutdownResult));
                }

                eosInitializedByBackend = false;
            }

            if (initialized) {
                RTB_INFO("OnlineSystem: EOS backend shut down.");
            }

            initialized = false;
        }

        bool EosOnlineBackend::IsInitialized() const
        {
            return initialized;
        }

        const char* EosOnlineBackend::GetLastError() const
        {
            return lastError.c_str();
        }

        IOnlineIdentity* EosOnlineBackend::GetIdentity()
        {
            return &identity;
        }

        const IOnlineIdentity* EosOnlineBackend::GetIdentity() const
        {
            return &identity;
        }

    }
}
