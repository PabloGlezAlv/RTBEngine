#pragma once

#include "../RTBEngineAPI.h"
#include "../Core/Event.h"
#include "OnlineResult.h"
#include "OnlineUser.h"

#include <string>

namespace RTBEngine {
    namespace Online {

        // Login methods the engine will support first.
        enum class OnlineLoginType {
            DeviceId,
            DeveloperAuth,
            AccountPortal
        };

        // Current state of the local user's identity session.
        enum class OnlineLoginStatus {
            NotLoggedIn,
            LoggingIn,
            LoggedIn,
            Error
        };

#pragma warning(push)
#pragma warning(disable: 4251)
        // Payload emitted whenever the local identity state changes.
        struct RTB_API OnlineLoginStatusChangedEvent {
            OnlineLoginStatus previousStatus = OnlineLoginStatus::NotLoggedIn;
            OnlineLoginStatus currentStatus = OnlineLoginStatus::NotLoggedIn;
            OnlineUserId localUserId;
        };

        struct RTB_API OnlineLoginOptions {
            OnlineLoginType type = OnlineLoginType::DeviceId;
            std::string displayName;
            std::string developerAuthHost = "localhost:6300";
            std::string developerAuthCredentialName;
        };

        // Identity contract for local-user login and user-id discovery.
        class RTB_API IOnlineIdentity {
        public:
            virtual ~IOnlineIdentity() = default;

            virtual OnlineResult Login(const OnlineLoginOptions& options) = 0;
            virtual void Logout() = 0;
            virtual OnlineLoginStatus GetLoginStatus() const = 0;
            virtual const OnlineUserId& GetLocalUserId() const = 0;
            virtual const std::string& GetDisplayName() const = 0;
            virtual const char* GetLastError() const = 0;
            virtual Core::EventSubscription SubscribeLoginStatusChanged(Core::Event<OnlineLoginStatusChangedEvent>::Callback callback) = 0;
            virtual void ClearLoginStatusChangedListeners() = 0;

            bool IsLoggedIn() const { return GetLoginStatus() == OnlineLoginStatus::LoggedIn; }
        };
#pragma warning(pop)

        RTB_API const char* ToString(OnlineLoginType type);
        RTB_API const char* ToString(OnlineLoginStatus status);

    }
}
