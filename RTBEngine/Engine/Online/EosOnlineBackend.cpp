#include "EosOnlineBackend.h"

#include "EosOnlineIdentity.h"
#include "EosOnlineLobby.h"
#include "EosP2PTransport.h"
#include "OnlineConfig.h"
#include "../Core/Logger.h"

#include <eos_common.h>
#include <eos_init.h>
#include <eos_logging.h>
#include <eos_sdk.h>
#include <eos_types.h>
#include <eos_ui.h>
#include <eos_version.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

    constexpr long long AuthUiWaitingLogCooldownMilliseconds = 30000;

    std::atomic<long long> gNextAuthUiWaitingLogAtMilliseconds{ 0 };
    std::atomic<int> gSuppressedAuthUiWaitingLogCount{ 0 };

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

    long long GetSteadyClockMilliseconds()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void ResetAuthUiLogState()
    {
        gNextAuthUiWaitingLogAtMilliseconds.store(0);
        gSuppressedAuthUiWaitingLogCount.store(0);
    }

    bool IsAuthUiWaitingLog(const std::string& text)
    {
        return text.find("corrective_action_required") != std::string::npos ||
            text.find("authorization_pending") != std::string::npos ||
            text.find("Account Portal overlay failed to load") != std::string::npos ||
            text.find("device auth continuation") != std::string::npos;
    }

    bool ShouldEmitAuthUiWaitingLog(std::string& text)
    {
        const long long now = GetSteadyClockMilliseconds();
        const long long nextLogTime = gNextAuthUiWaitingLogAtMilliseconds.load();
        if (now < nextLogTime) {
            gSuppressedAuthUiWaitingLogCount.fetch_add(1);
            return false;
        }

        const int suppressedCount = gSuppressedAuthUiWaitingLogCount.exchange(0);
        if (suppressedCount > 0) {
            text += " (suppressed ";
            text += std::to_string(suppressedCount);
            text += " duplicate EOS Auth UI polling messages)";
        }

        gNextAuthUiWaitingLogAtMilliseconds.store(
            now + AuthUiWaitingLogCooldownMilliseconds);
        return true;
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

        if (IsAuthUiWaitingLog(text)) {
            if (!ShouldEmitAuthUiWaitingLog(text)) {
                return;
            }
        }

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

        EosOnlineBackend::EosOnlineBackend()
            : identity(std::make_unique<EosOnlineIdentity>())
            , lobby(std::make_unique<EosOnlineLobby>(identity.get()))
            , transport(std::make_unique<EosP2PTransport>(identity.get()))
        {
        }

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
            // Ignore repeated initialization attempts.
            if (initialized) {
                return true;
            }

            // Start each initialization attempt with a clean error state.
            lastError.clear();
            ResetAuthUiLogState();

            // Validate the EOS portal data before calling into the SDK.
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

            // Track ownership so shutdown does not tear down EOS configured elsewhere.
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
            platformOptions.CacheDirectory = config.cacheDirectory.empty() ? nullptr : config.cacheDirectory.c_str();
            platformOptions.TickBudgetInMilliseconds = config.tickBudgetMilliseconds;

            if (config.loadingInEditor) {
                // Let EOS know this platform instance is running inside an editor-like host.
                platformOptions.Flags |= EOS_PF_LOADING_IN_EDITOR;
            }

            const bool disableOverlayForThisPlatform =
                config.disableOverlay || config.loadingInEditor || config.isServer;
            authOverlayEnabled = !disableOverlayForThisPlatform;
            if (disableOverlayForThisPlatform) {
                // Editor and server/headless instances should never open Epic UI.
                platformOptions.Flags |= EOS_PF_DISABLE_OVERLAY;
                platformOptions.Flags |= EOS_PF_DISABLE_SOCIAL_OVERLAY;
            } else {
                // Test clients need the overlay for EAS corrective-action prompts.
                platformOptions.Flags |= EOS_PF_WINDOWS_ENABLE_OVERLAY_D3D9;
                platformOptions.Flags |= EOS_PF_WINDOWS_ENABLE_OVERLAY_D3D10;
                platformOptions.Flags |= EOS_PF_WINDOWS_ENABLE_OVERLAY_OPENGL;
                RTB_INFO("OnlineSystem: EOS overlay enabled for client platform.");
            }

            // Create the platform instance used by Connect, Lobby, P2P, and future EOS services.
            platformHandle = EOS_Platform_Create(&platformOptions);
            if (!platformHandle) {
                lastError = "EOS_Platform_Create failed.";
                RTB_ERROR("OnlineSystem: " + lastError);
                Shutdown();
                return false;
            }

            RegisterUiDiagnostics();

            // Give identity access to the platform so it can resolve EOS Connect.
            identity->SetAuthOverlayDiagnosticsEnabled(authOverlayEnabled);
            identity->SetPlatformHandle(platformHandle);
            lobby->SetPlatformHandle(platformHandle);
            transport->SetPlatformHandle(platformHandle);

            initialized = true;
            RTB_INFO(std::string("OnlineSystem: EOS backend initialized. SDK version: ") + EOS_GetVersion());
            return true;
        }

        // EOS processes asynchronous SDK work from this call.
        // Future login, lobby, and P2P callbacks will depend on this running every frame.
        void EosOnlineBackend::Tick(float deltaTime)
        {
            if (!platformHandle) {
                return;
            }

            EOS_Platform_Tick(static_cast<EOS_HPlatform>(platformHandle));

            if (lobby) {
                lobby->Tick(deltaTime);
            }
        }

        // Releases the platform handle before shutting down the global EOS SDK state.
        void EosOnlineBackend::Shutdown()
        {
            // Identity must forget EOS handles before the platform is released.
            if (lobby) {
                lobby->ResetPlatformHandle();
            }

            if (transport) {
                transport->ResetPlatformHandle();
            }

            if (identity) {
                identity->ResetPlatformHandle();
            }

            RemoveUiDiagnostics();

            // Release the platform handle before EOS_Shutdown as required by EOS.
            if (platformHandle) {
                EOS_Platform_Release(static_cast<EOS_HPlatform>(platformHandle));
                platformHandle = nullptr;
            }

            // Only shut down the global SDK if this backend initialized it.
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
            authOverlayEnabled = false;
            ResetAuthUiLogState();
        }

        void EosOnlineBackend::RegisterUiDiagnostics()
        {
            if (!platformHandle || uiDisplaySettingsNotificationId != EOS_INVALID_NOTIFICATIONID) {
                return;
            }

            uiHandle = EOS_Platform_GetUIInterface(static_cast<EOS_HPlatform>(platformHandle));
            if (!uiHandle) {
                RTB_WARN("OnlineSystem: EOS UI interface is not available; overlay diagnostics disabled.");
                return;
            }

            EOS_UI_AddNotifyDisplaySettingsUpdatedOptions options{};
            options.ApiVersion = EOS_UI_ADDNOTIFYDISPLAYSETTINGSUPDATED_API_LATEST;
            uiDisplaySettingsNotificationId =
                EOS_UI_AddNotifyDisplaySettingsUpdated(
                    static_cast<EOS_HUI>(uiHandle),
                    &options,
                    this,
                    &EosOnlineBackend::OnUiDisplaySettingsUpdated);

            if (uiDisplaySettingsNotificationId == EOS_INVALID_NOTIFICATIONID) {
                RTB_WARN("OnlineSystem: EOS_UI_AddNotifyDisplaySettingsUpdated returned an invalid notification id.");
            } else {
                RTB_INFO("OnlineSystem: EOS overlay diagnostics registered.");
            }
        }

        void EosOnlineBackend::RemoveUiDiagnostics()
        {
            if (uiHandle && uiDisplaySettingsNotificationId != EOS_INVALID_NOTIFICATIONID) {
                EOS_UI_RemoveNotifyDisplaySettingsUpdated(
                    static_cast<EOS_HUI>(uiHandle),
                    uiDisplaySettingsNotificationId);
            }

            uiDisplaySettingsNotificationId = EOS_INVALID_NOTIFICATIONID;
            uiHandle = nullptr;
            uiDisplayStateKnown = false;
            eosOverlayVisible = false;
            eosOverlayExclusiveInput = false;
        }

        void EOS_CALL EosOnlineBackend::OnUiDisplaySettingsUpdated(
            const EOS_UI_OnDisplaySettingsUpdatedCallbackInfo* data)
        {
            if (!data || !data->ClientData) {
                return;
            }

            EosOnlineBackend* self = static_cast<EosOnlineBackend*>(data->ClientData);
            const bool visible = data->bIsVisible == EOS_TRUE;
            const bool exclusiveInput = data->bIsExclusiveInput == EOS_TRUE;
            const bool changed =
                !self->uiDisplayStateKnown ||
                self->eosOverlayVisible != visible ||
                self->eosOverlayExclusiveInput != exclusiveInput;

            self->uiDisplayStateKnown = true;
            self->eosOverlayVisible = visible;
            self->eosOverlayExclusiveInput = exclusiveInput;

            if (changed) {
                const bool handledByIdentity =
                    self->identity &&
                    self->identity->NotifyAuthOverlayDisplayState(visible, exclusiveInput);

                if (handledByIdentity) {
                    return;
                }

                RTB_INFO(std::string("OnlineSystem: EOS overlay display state. Visible: ") +
                    (visible ? "true" : "false") +
                    " ExclusiveInput: " + (exclusiveInput ? "true" : "false"));
            }
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
            return identity.get();
        }

        const IOnlineIdentity* EosOnlineBackend::GetIdentity() const
        {
            return identity.get();
        }

        IOnlineLobby* EosOnlineBackend::GetLobby()
        {
            return lobby.get();
        }

        const IOnlineLobby* EosOnlineBackend::GetLobby() const
        {
            return lobby.get();
        }

        IOnlineTransport* EosOnlineBackend::GetTransport()
        {
            return transport.get();
        }

        const IOnlineTransport* EosOnlineBackend::GetTransport() const
        {
            return transport.get();
        }

    }
}
