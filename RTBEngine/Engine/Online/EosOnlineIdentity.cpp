#include "EosOnlineIdentity.h"

#include "../Core/Logger.h"

#include <eos_common.h>
#include <eos_connect.h>
#include <eos_connect_types.h>
#include <eos_sdk.h>

#include <memory>
#include <string>
#include <utility>

namespace {

    std::string EosResultToString(EOS_EResult result)
    {
        // Ask EOS for a readable name for the result code.
        const char* resultText = EOS_EResult_ToString(result);
        return resultText ? resultText : "EOS_Unknown";
    }

    std::string ProductUserIdToString(EOS_ProductUserId productUserId)
    {
        // Allocate the exact buffer size EOS documents for Product User IDs.
        char buffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1]{};
        int32_t bufferLength = static_cast<int32_t>(sizeof(buffer));

        // Convert the opaque EOS handle into a string the engine can store safely.
        const EOS_EResult result = EOS_ProductUserId_ToString(productUserId, buffer, &bufferLength);
        if (result != EOS_EResult::EOS_Success) {
            return {};
        }

        return buffer;
    }

}

namespace RTBEngine {
    namespace Online {

        class EosOnlineIdentity::Impl {
        public:
            void SetPlatformHandle(void* handle)
            {
                // Store the platform and resolve the Connect interface from it.
                platformHandle = handle;
                connectHandle = platformHandle
                    ? EOS_Platform_GetConnectInterface(static_cast<EOS_HPlatform>(platformHandle))
                    : nullptr;
            }

            void ResetPlatformHandle()
            {
                // Drop EOS handles first so future calls cannot use stale SDK state.
                platformHandle = nullptr;
                connectHandle = nullptr;
                Logout();
            }

            OnlineResult Login(const OnlineLoginOptions& options)
            {
                // Login cannot start until the backend has created an EOS platform.
                if (!connectHandle) {
                    FailLogin("EOS Connect interface is not available.", OnlineErrorCode::InvalidState);
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                // Avoid starting two asynchronous login flows at the same time.
                if (status == OnlineLoginStatus::LoggingIn) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "EOS identity login is already in progress.");
                }

                // This step only supports anonymous Device ID login.
                if (options.type != OnlineLoginType::DeviceId) {
                    FailLogin("Only EOS Connect Device ID login is implemented in this step.", OnlineErrorCode::NotImplemented);
                    return OnlineResult::Failure(OnlineErrorCode::NotImplemented, lastError);
                }

                // Reset local identity state before starting the async EOS flow.
                displayName = options.displayName.empty() ? "RTBEnginePlayer" : options.displayName;
                lastError.clear();
                localUserId = OnlineUserId();
                localProductUserId = nullptr;
                SetStatus(OnlineLoginStatus::LoggingIn);

                // Kick off the first async EOS call.
                StartDeviceIdCreation();
                return OnlineResult::Success("EOS Device ID login started.");
            }

            void Logout()
            {
                const bool hadActiveState = status != OnlineLoginStatus::NotLoggedIn;

                // Tell EOS to discard the local Product User session if one exists.
                if (connectHandle && localProductUserId) {
                    EOS_Connect_LogoutOptions options{};
                    options.ApiVersion = EOS_CONNECT_LOGOUT_API_LATEST;
                    options.LocalUserId = localProductUserId;
                    EOS_Connect_Logout(connectHandle, &options, nullptr, &Impl::OnLogoutCompleted);
                }

                // Clear engine-side identity state immediately.
                localUserId = OnlineUserId();
                localProductUserId = nullptr;
                displayName.clear();
                lastError.clear();

                if (hadActiveState) {
                    SetStatus(OnlineLoginStatus::NotLoggedIn);
                } else {
                    status = OnlineLoginStatus::NotLoggedIn;
                }
            }

            OnlineLoginStatus GetLoginStatus() const
            {
                return status;
            }

            const OnlineUserId& GetLocalUserId() const
            {
                return localUserId;
            }

            const std::string& GetDisplayName() const
            {
                return displayName;
            }

            const char* GetLastError() const
            {
                return lastError.c_str();
            }

            Core::EventSubscription SubscribeLoginStatusChanged(Core::Event<OnlineLoginStatusChangedEvent>::Callback callback)
            {
                // Register an external listener for async EOS identity state changes.
                return loginStatusChanged.Subscribe(std::move(callback));
            }

            void ClearLoginStatusChangedListeners()
            {
                // Drop all listeners owned by this identity instance.
                loginStatusChanged.Clear();
            }

        private:
            void StartDeviceIdCreation()
            {
                // Create or reuse EOS's anonymous local-device credentials.
                EOS_Connect_CreateDeviceIdOptions options{};
                options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
                options.DeviceModel = "RTBEngine Windows";

                // EOS will call OnCreateDeviceIdCompleted from EOS_Platform_Tick.
                EOS_Connect_CreateDeviceId(connectHandle, &options, this, &Impl::OnCreateDeviceIdCompleted);
            }

            void StartDeviceIdLogin()
            {
                // Device ID login uses EOS's locally stored access token.
                EOS_Connect_Credentials credentials{};
                credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
                credentials.Token = nullptr;
                credentials.Type = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;

                // EOS requires user login info for Device ID login.
                EOS_Connect_UserLoginInfo userLoginInfo{};
                userLoginInfo.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
                userLoginInfo.DisplayName = displayName.c_str();

                // Build the Connect login request.
                EOS_Connect_LoginOptions options{};
                options.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
                options.Credentials = &credentials;
                options.UserLoginInfo = &userLoginInfo;

                // EOS will call OnLoginCompleted from EOS_Platform_Tick.
                EOS_Connect_Login(connectHandle, &options, this, &Impl::OnLoginCompleted);
            }

            void StartCreateUser(EOS_ContinuanceToken continuanceToken)
            {
                // Use the continuance token from Login to create a new Product User.
                EOS_Connect_CreateUserOptions options{};
                options.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
                options.ContinuanceToken = continuanceToken;

                // EOS will call OnCreateUserCompleted from EOS_Platform_Tick.
                EOS_Connect_CreateUser(connectHandle, &options, this, &Impl::OnCreateUserCompleted);
            }

            void CompleteLogin(EOS_ProductUserId productUserId)
            {
                // Convert EOS's opaque user handle to the engine's serializable id.
                const std::string productUserIdText = ProductUserIdToString(productUserId);
                if (productUserIdText.empty()) {
                    FailLogin("EOS returned an invalid Product User ID.", OnlineErrorCode::BackendError);
                    return;
                }

                // Store both the EOS handle and the engine-facing user id.
                localProductUserId = productUserId;
                localUserId = OnlineUserId(OnlineUserIdType::EOSProductUser, productUserIdText);
                lastError.clear();
                SetStatus(OnlineLoginStatus::LoggedIn);

                RTB_INFO("OnlineIdentity: EOS Device ID login completed. ProductUserId: " + productUserIdText);
            }

            void FailLogin(const std::string& message, OnlineErrorCode)
            {
                // Move identity into an error state and clear any partial user data.
                localProductUserId = nullptr;
                localUserId = OnlineUserId();
                lastError = message;
                SetStatus(OnlineLoginStatus::Error);
                RTB_ERROR("OnlineIdentity: " + lastError);
            }

            void SetStatus(OnlineLoginStatus newStatus)
            {
                const OnlineLoginStatus previousStatus = status;
                status = newStatus;

                if (previousStatus == newStatus) {
                    return;
                }

                loginStatusChanged.Invoke({ previousStatus, status, localUserId });
            }

            static void EOS_CALL OnCreateDeviceIdCompleted(const EOS_Connect_CreateDeviceIdCallbackInfo* data)
            {
                // EOS returns our Impl pointer through ClientData.
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                // DuplicateNotAllowed means credentials already exist, so login can continue.
                if (data->ResultCode == EOS_EResult::EOS_Success ||
                    data->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed) {
                    self->StartDeviceIdLogin();
                    return;
                }

                self->FailLogin(
                    "EOS_Connect_CreateDeviceId failed: " + EosResultToString(data->ResultCode),
                    OnlineErrorCode::BackendError
                );
            }

            static void EOS_CALL OnLoginCompleted(const EOS_Connect_LoginCallbackInfo* data)
            {
                // EOS returns our Impl pointer through ClientData.
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                // Successful login gives us the local Product User ID immediately.
                if (data->ResultCode == EOS_EResult::EOS_Success) {
                    self->CompleteLogin(data->LocalUserId);
                    return;
                }

                // First-time Device ID users may need a Product User created.
                if (data->ResultCode == EOS_EResult::EOS_InvalidUser && data->ContinuanceToken) {
                    self->StartCreateUser(data->ContinuanceToken);
                    return;
                }

                self->FailLogin(
                    "EOS_Connect_Login failed: " + EosResultToString(data->ResultCode),
                    OnlineErrorCode::BackendError
                );
            }

            static void EOS_CALL OnCreateUserCompleted(const EOS_Connect_CreateUserCallbackInfo* data)
            {
                // EOS returns our Impl pointer through ClientData.
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                // User creation completes the same login flow.
                if (data->ResultCode == EOS_EResult::EOS_Success) {
                    self->CompleteLogin(data->LocalUserId);
                    return;
                }

                self->FailLogin(
                    "EOS_Connect_CreateUser failed: " + EosResultToString(data->ResultCode),
                    OnlineErrorCode::BackendError
                );
            }

            static void EOS_CALL OnLogoutCompleted(const EOS_Connect_LogoutCallbackInfo* data)
            {
                // Logout is best-effort because engine state has already been cleared.
                if (!data) {
                    return;
                }

                if (data->ResultCode != EOS_EResult::EOS_Success) {
                    RTB_WARN("OnlineIdentity: EOS_Connect_Logout returned " + EosResultToString(data->ResultCode));
                }
            }

            void* platformHandle = nullptr;
            EOS_HConnect connectHandle = nullptr;
            EOS_ProductUserId localProductUserId = nullptr;
            OnlineLoginStatus status = OnlineLoginStatus::NotLoggedIn;
            OnlineUserId localUserId;
            std::string displayName;
            std::string lastError;
            Core::Event<OnlineLoginStatusChangedEvent> loginStatusChanged;
        };

        EosOnlineIdentity::EosOnlineIdentity()
            : impl(std::make_unique<Impl>())
        {
        }

        EosOnlineIdentity::~EosOnlineIdentity() = default;

        void EosOnlineIdentity::SetPlatformHandle(void* handle)
        {
            // Forward the backend-created EOS platform to the private implementation.
            impl->SetPlatformHandle(handle);
        }

        void EosOnlineIdentity::ResetPlatformHandle()
        {
            // Clear EOS handles before the backend releases the platform.
            impl->ResetPlatformHandle();
        }

        OnlineResult EosOnlineIdentity::Login(const OnlineLoginOptions& options)
        {
            // Keep the public method thin; all EOS details live in Impl.
            return impl->Login(options);
        }

        void EosOnlineIdentity::Logout()
        {
            // Keep logout behavior centralized in Impl.
            impl->Logout();
        }

        OnlineLoginStatus EosOnlineIdentity::GetLoginStatus() const
        {
            return impl->GetLoginStatus();
        }

        const OnlineUserId& EosOnlineIdentity::GetLocalUserId() const
        {
            return impl->GetLocalUserId();
        }

        const std::string& EosOnlineIdentity::GetDisplayName() const
        {
            return impl->GetDisplayName();
        }

        const char* EosOnlineIdentity::GetLastError() const
        {
            return impl->GetLastError();
        }

        Core::EventSubscription EosOnlineIdentity::SubscribeLoginStatusChanged(Core::Event<OnlineLoginStatusChangedEvent>::Callback callback)
        {
            // Forward listener registration to the private implementation.
            return impl->SubscribeLoginStatusChanged(std::move(callback));
        }

        void EosOnlineIdentity::ClearLoginStatusChangedListeners()
        {
            // Forward listener clearing to the private implementation.
            impl->ClearLoginStatusChangedListeners();
        }

    }
}
