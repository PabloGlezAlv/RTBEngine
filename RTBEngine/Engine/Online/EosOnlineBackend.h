#pragma once

#include "IOnlineBackend.h"

#include <eos_common.h>
#include <eos_ui_types.h>

#include <memory>
#include <string>

namespace RTBEngine {
    namespace Online {

        class EosOnlineIdentity;
        class EosOnlineLobby;
        class EosP2PTransport;

#pragma warning(push)
#pragma warning(disable: 4251)
        // EOS SDK backend.
        // This first implementation only owns SDK startup, platform ticking, and shutdown.
        class RTB_API EosOnlineBackend final : public IOnlineBackend {
        public:
            EosOnlineBackend();
            ~EosOnlineBackend() override;

            const char* GetName() const override;
            bool Initialize(const OnlineConfig& config) override;
            void Tick(float deltaTime) override;
            void Shutdown() override;
            bool IsInitialized() const override;
            const char* GetLastError() const override;
            IOnlineIdentity* GetIdentity() override;
            const IOnlineIdentity* GetIdentity() const override;
            IOnlineLobby* GetLobby() override;
            const IOnlineLobby* GetLobby() const override;
            IOnlineTransport* GetTransport() override;
            const IOnlineTransport* GetTransport() const override;

        private:
            void RegisterUiDiagnostics();
            void RemoveUiDiagnostics();
            static void EOS_CALL OnUiDisplaySettingsUpdated(const EOS_UI_OnDisplaySettingsUpdatedCallbackInfo* data);

            // Stored as void* to keep EOS types out of the public engine header surface.
            void* platformHandle = nullptr;
            void* uiHandle = nullptr;
            EOS_NotificationId uiDisplaySettingsNotificationId = EOS_INVALID_NOTIFICATIONID;

            // Tracks ownership so we only call EOS_Shutdown if this backend called EOS_Initialize.
            bool eosInitializedByBackend = false;
            bool initialized = false;
            bool authOverlayEnabled = false;
            bool uiDisplayStateKnown = false;
            bool eosOverlayVisible = false;
            bool eosOverlayExclusiveInput = false;
            std::string lastError;
            std::unique_ptr<EosOnlineIdentity> identity;
            std::unique_ptr<EosOnlineLobby> lobby;
            std::unique_ptr<EosP2PTransport> transport;
        };
#pragma warning(pop)

    }
}
