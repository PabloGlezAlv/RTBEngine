#pragma once

#include "IOnlineIdentity.h"

#include <memory>

namespace RTBEngine {
    namespace Online {

        class EosOnlineBackend;

#pragma warning(push)
#pragma warning(disable: 4251)
        // EOS Connect identity implementation.
        // The public header stays provider-neutral; EOS-native handles live in the private Impl.
        class RTB_API EosOnlineIdentity final : public IOnlineIdentity {
        public:
            EosOnlineIdentity();
            ~EosOnlineIdentity() override;

            void SetPlatformHandle(void* handle);
            void ResetPlatformHandle();

            OnlineResult Login(const OnlineLoginOptions& options) override;
            void Logout() override;
            OnlineLoginStatus GetLoginStatus() const override;
            const OnlineUserId& GetLocalUserId() const override;
            const std::string& GetDisplayName() const override;
            const char* GetLastError() const override;
            Core::EventSubscription SubscribeLoginStatusChanged(Core::Event<OnlineLoginStatusChangedEvent>::Callback callback) override;
            void ClearLoginStatusChangedListeners() override;

        private:
            friend class EosOnlineBackend;

            void SetAuthOverlayDiagnosticsEnabled(bool enabled);
            bool NotifyAuthOverlayDisplayState(bool visible, bool exclusiveInput);

            class Impl;
            std::unique_ptr<Impl> impl;
        };
#pragma warning(pop)

    }
}
