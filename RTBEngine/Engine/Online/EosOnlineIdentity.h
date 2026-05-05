#pragma once

#include "IOnlineIdentity.h"

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API EosOnlineIdentity final : public IOnlineIdentity {
        public:
            void SetPlatformHandle(void* handle);
            void ResetPlatformHandle();

            OnlineResult Login(const OnlineLoginOptions& options) override;
            void Logout() override;
            OnlineLoginStatus GetLoginStatus() const override;
            const OnlineUserId& GetLocalUserId() const override;
            const std::string& GetDisplayName() const override;
            const char* GetLastError() const override;

        private:
            void* platformHandle = nullptr;
            OnlineLoginStatus status = OnlineLoginStatus::NotLoggedIn;
            OnlineUserId localUserId;
            std::string displayName;
            std::string lastError;
        };
#pragma warning(pop)

    }
}
