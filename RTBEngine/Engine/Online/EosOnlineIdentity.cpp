#include "EosOnlineIdentity.h"

#include "../Core/Logger.h"

#include <eos_auth.h>
#include <eos_auth_types.h>
#include <eos_common.h>
#include <eos_connect.h>
#include <eos_connect_types.h>
#include <eos_sdk.h>

#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace {

    constexpr int AuthOverlayBounceTransitionLimit = 8;
    constexpr long long AuthOverlayBounceTimeoutMilliseconds = 60000;

    long long GetSteadyClockMilliseconds()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

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

    std::string EpicAccountIdToString(EOS_EpicAccountId epicAccountId)
    {
        char buffer[EOS_EPICACCOUNTID_MAX_LENGTH + 1]{};
        int32_t bufferLength = static_cast<int32_t>(sizeof(buffer));

        const EOS_EResult result = EOS_EpicAccountId_ToString(epicAccountId, buffer, &bufferLength);
        if (result != EOS_EResult::EOS_Success) {
            return {};
        }

        return buffer;
    }

    const char* AuthLoginStatusToString(EOS_ELoginStatus status)
    {
        switch (status) {
        case EOS_ELoginStatus::EOS_LS_NotLoggedIn:
            return "NotLoggedIn";
        case EOS_ELoginStatus::EOS_LS_UsingLocalProfile:
            return "UsingLocalProfile";
        case EOS_ELoginStatus::EOS_LS_LoggedIn:
            return "LoggedIn";
        default:
            return "Unknown";
        }
    }

    std::string BuildAuthConfigurationChecklist()
    {
        return " Checklist: verify the DevAuthTool host/port and credential name, "
            "the Epic account has access to the organization/product, an Epic Account "
            "Services Application exists, BasicProfile is allowed in Application "
            "Permissions, and the EOS Client is linked to that Application.";
    }

    std::string BuildAuthFailureMessage(
        RTBEngine::Online::OnlineLoginType loginType,
        EOS_EResult result,
        const std::string& credentialName,
        const std::string& host)
    {
        std::string message = std::string("EOS_Auth_Login ") +
            RTBEngine::Online::ToString(loginType) + " failed";

        if (loginType == RTBEngine::Online::OnlineLoginType::DeveloperAuth) {
            message += " for credential '" + credentialName + "' at '" + host + "'";
        }

        message += ": " + EosResultToString(result);

        if (result == EOS_EResult::EOS_InvalidRequest) {
            message += ". Check that the EOS Client Id is linked to an Epic Account Services "
                "Application in the Developer Portal.";
        }

        if (result == EOS_EResult::EOS_Auth_UserInterfaceRequired) {
            message += ". EOS needs the user to complete an Epic Account Services prompt. "
                "Configure Application Permissions in the Developer Portal and allow the EOS prompt.";
        }

        if (result == EOS_EResult::EOS_Auth_CorrectiveActionRequired) {
            message += ". EOS still reports corrective action after the prompt. Check EAS "
                "Application permissions, account organization access, Brand Settings if needed, "
                "and recreate the DevAuth credential after portal changes.";
        }

        if (result == EOS_EResult::EOS_Auth_AccountPortalLoadError) {
            message += ". EOS could not load the Account Portal. Check overlay/browser availability, "
                "network access, and EOS overlay configuration.";
        }

        if (result == EOS_EResult::EOS_Auth_PinGrantCode) {
            message += ". EOS returned a PIN grant code; complete browser/device authorization.";
        }

        if (result == EOS_EResult::EOS_Auth_PinGrantPending) {
            message += ". EOS is still waiting for browser/device authorization.";
        }

        if (result == EOS_EResult::EOS_Auth_PinGrantExpired) {
            message += ". EOS browser/device authorization expired; restart login.";
        }

        if (loginType == RTBEngine::Online::OnlineLoginType::DeveloperAuth ||
            loginType == RTBEngine::Online::OnlineLoginType::AccountPortal) {
            message += BuildAuthConfigurationChecklist();
        }

        return message;
    }

    std::string BuildAuthOverlayBounceFailureMessage(
        RTBEngine::Online::OnlineLoginType loginType,
        const std::string& credentialName,
        const std::string& host,
        int transitionCount,
        long long elapsedMilliseconds)
    {
        std::string message = std::string("EOS Auth overlay kept reopening/closing during ") +
            RTBEngine::Online::ToString(loginType) + " login";

        if (loginType == RTBEngine::Online::OnlineLoginType::DeveloperAuth) {
            message += " for credential '" + credentialName + "' at '" + host + "'";
        }

        message += ". Transitions: " + std::to_string(transitionCount) +
            " ElapsedMs: " + std::to_string(elapsedMilliseconds);

        if (loginType == RTBEngine::Online::OnlineLoginType::DeveloperAuth) {
            message += ". DevAuthTool was reached, so the likely failure is after DevAuth, "
                "inside Epic Account Services consent/configuration.";
        } else {
            message += ". The likely failure is inside Epic Account Services "
                "consent/configuration.";
        }

        message += BuildAuthConfigurationChecklist();
        return message;
    }

    std::string BuildPinGrantMessage(const EOS_Auth_PinGrantInfo* pinGrantInfo)
    {
        std::string message = "OnlineIdentity: EOS requires browser/device authorization.";

        if (!pinGrantInfo) {
            return message;
        }

        if (pinGrantInfo->VerificationURIComplete && pinGrantInfo->VerificationURIComplete[0] != '\0') {
            message += " Open: ";
            message += pinGrantInfo->VerificationURIComplete;
        } else if (pinGrantInfo->VerificationURI && pinGrantInfo->VerificationURI[0] != '\0') {
            message += " Open: ";
            message += pinGrantInfo->VerificationURI;
        }

        if (pinGrantInfo->UserCode && pinGrantInfo->UserCode[0] != '\0') {
            message += " Code: ";
            message += pinGrantInfo->UserCode;
        }

        if (pinGrantInfo->ExpiresIn > 0) {
            message += " ExpiresInSeconds: ";
            message += std::to_string(pinGrantInfo->ExpiresIn);
        }

        return message;
    }

    std::string BuildIntermediateAuthMessage(
        RTBEngine::Online::OnlineLoginType loginType,
        EOS_EResult result)
    {
        std::string message = std::string("OnlineIdentity: EOS_Auth_Login ") +
            RTBEngine::Online::ToString(loginType) + " is waiting on user interaction: " +
            EosResultToString(result);

        if (result == EOS_EResult::EOS_Auth_CorrectiveActionRequired) {
            message += ". Complete the Epic Account Services prompt; the login will keep waiting instead of restarting.";
        } else if (result == EOS_EResult::EOS_Auth_PinGrantPending) {
            message += ". Waiting for the pending Epic authorization flow.";
        }

        return message;
    }

    bool IsFinalAuthFailureResult(EOS_EResult result)
    {
        return result == EOS_EResult::EOS_Auth_UserInterfaceRequired ||
            result == EOS_EResult::EOS_Auth_AccountPortalLoadError ||
            result == EOS_EResult::EOS_Auth_PinGrantExpired;
    }

}

namespace RTBEngine {
    namespace Online {

        class EosOnlineIdentity::Impl {
        public:
            void SetPlatformHandle(void* handle)
            {
                RemoveAuthLoginStatusNotification();

                // Store the platform and resolve the Connect interface from it.
                platformHandle = handle;
                authHandle = platformHandle
                    ? EOS_Platform_GetAuthInterface(static_cast<EOS_HPlatform>(platformHandle))
                    : nullptr;
                connectHandle = platformHandle
                    ? EOS_Platform_GetConnectInterface(static_cast<EOS_HPlatform>(platformHandle))
                    : nullptr;

                AddAuthLoginStatusNotification();
            }

            void SetAuthOverlayDiagnosticsEnabled(bool enabled)
            {
                authOverlayDiagnosticsEnabled = enabled;
            }

            bool NotifyAuthOverlayDisplayState(bool visible, bool exclusiveInput)
            {
                if (!authLoginInFlight || status != OnlineLoginStatus::LoggingIn) {
                    return false;
                }

                const bool changed =
                    !authOverlayStateKnown ||
                    authOverlayVisible != visible ||
                    authOverlayExclusiveInput != exclusiveInput;

                authOverlayStateKnown = true;
                authOverlayVisible = visible;
                authOverlayExclusiveInput = exclusiveInput;

                if (!changed) {
                    return true;
                }

                ++authOverlayTransitionCount;
                const long long elapsedMilliseconds = GetAuthAttemptElapsedMilliseconds();

                RTB_INFO(GetAuthAttemptPrefix() + " overlay transition #" +
                    std::to_string(authOverlayTransitionCount) +
                    ". Visible: " + (visible ? "true" : "false") +
                    " ExclusiveInput: " + (exclusiveInput ? "true" : "false") +
                    " ElapsedMs: " + std::to_string(elapsedMilliseconds));

                if (!authOverlayBounceFailed &&
                    (authOverlayTransitionCount > AuthOverlayBounceTransitionLimit ||
                        elapsedMilliseconds > AuthOverlayBounceTimeoutMilliseconds)) {
                    authOverlayBounceFailed = true;
                    FailLogin(
                        BuildAuthOverlayBounceFailureMessage(
                            activeLoginType,
                            developerAuthCredentialName,
                            developerAuthHost,
                            authOverlayTransitionCount,
                            elapsedMilliseconds),
                        OnlineErrorCode::BackendError);
                }

                return true;
            }

            void ResetPlatformHandle()
            {
                Logout();
                RemoveAuthLoginStatusNotification();

                // Drop EOS handles so future calls cannot use stale SDK state.
                platformHandle = nullptr;
                authHandle = nullptr;
                connectHandle = nullptr;
            }

            OnlineResult Login(const OnlineLoginOptions& options)
            {
                // Login cannot start until the backend has created an EOS platform.
                if (!connectHandle) {
                    FailLogin("EOS Connect interface is not available.", OnlineErrorCode::InvalidState);
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                // Avoid starting two asynchronous login flows at the same time.
                if (status == OnlineLoginStatus::LoggingIn || authLoginInFlight) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "EOS identity login is already in progress.");
                }

                const bool usesAuthInterface =
                    options.type == OnlineLoginType::DeveloperAuth ||
                    options.type == OnlineLoginType::AccountPortal;
                if (usesAuthInterface && !authHandle) {
                    FailLogin("EOS Auth interface is not available.", OnlineErrorCode::InvalidState);
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                if (options.type == OnlineLoginType::DeveloperAuth &&
                    (options.developerAuthHost.empty() || options.developerAuthCredentialName.empty())) {
                    FailLogin("DeveloperAuth needs host and credential name.", OnlineErrorCode::InvalidConfig);
                    return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, lastError);
                }

                // Reset local identity state before starting the async EOS flow.
                activeLoginType = options.type;
                displayName = options.displayName.empty() ? "RTBEnginePlayer" : options.displayName;
                lastError.clear();
                localUserId = OnlineUserId();
                localEpicAccountId = nullptr;
                localProductUserId = nullptr;
                connectLoginToken.clear();
                SetStatus(OnlineLoginStatus::LoggingIn);

                if (options.type == OnlineLoginType::DeveloperAuth) {
                    StartDeveloperAuthLogin(options);
                    return OnlineResult::Success("EOS Developer Auth login started.");
                }

                if (options.type == OnlineLoginType::AccountPortal) {
                    StartAccountPortalLogin();
                    return OnlineResult::Success("EOS Account Portal login started.");
                }

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

                // DeveloperAuth goes through EOS Auth before Connect, so close that session too.
                if (authHandle && localEpicAccountId) {
                    EOS_Auth_LogoutOptions options{};
                    options.ApiVersion = EOS_AUTH_LOGOUT_API_LATEST;
                    options.LocalUserId = localEpicAccountId;
                    EOS_Auth_Logout(authHandle, &options, nullptr, &Impl::OnAuthLogoutCompleted);
                }

                // Clear engine-side identity state immediately.
                EndAuthLoginAttempt("Canceled");
                localUserId = OnlineUserId();
                localEpicAccountId = nullptr;
                localProductUserId = nullptr;
                displayName.clear();
                lastError.clear();
                connectLoginToken.clear();

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

            void StartDeveloperAuthLogin(const OnlineLoginOptions& loginOptions)
            {
                developerAuthHost = loginOptions.developerAuthHost;
                developerAuthCredentialName = loginOptions.developerAuthCredentialName;
                BeginAuthLoginAttempt();

                // DevAuthTool credentials are consumed by EOS Auth, not directly by Connect.
                EOS_Auth_Credentials credentials{};
                credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
                credentials.Id = developerAuthHost.c_str();
                credentials.Token = developerAuthCredentialName.c_str();
                credentials.Type = EOS_ELoginCredentialType::EOS_LCT_Developer;

                RTB_INFO("OnlineIdentity: starting EOS DeveloperAuth login. Host: " +
                    developerAuthHost + " Credential: " + developerAuthCredentialName);

                // Basic profile is enough for development login and later ID-token handoff to Connect.
                EOS_Auth_LoginOptions options{};
                options.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
                options.Credentials = &credentials;
                options.ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_BasicProfile;
                options.LoginFlags = 0;

                // EOS will call OnAuthLoginCompleted from EOS_Platform_Tick.
                EOS_Auth_Login(authHandle, &options, this, &Impl::OnAuthLoginCompleted);
            }

            void StartAccountPortalLogin()
            {
                developerAuthHost.clear();
                developerAuthCredentialName.clear();
                BeginAuthLoginAttempt();

                EOS_Auth_Credentials credentials{};
                credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
                credentials.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;

                RTB_INFO("OnlineIdentity: starting EOS AccountPortal login.");

                EOS_Auth_LoginOptions options{};
                options.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
                options.Credentials = &credentials;
                options.ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_BasicProfile;
                options.LoginFlags = 0;

                // EOS will call OnAuthLoginCompleted from EOS_Platform_Tick.
                EOS_Auth_Login(authHandle, &options, this, &Impl::OnAuthLoginCompleted);
            }

            void BeginAuthLoginAttempt()
            {
                ++authAttemptId;
                authLoginInFlight = true;
                authAttemptStartMilliseconds = GetSteadyClockMilliseconds();
                authPinGrantLogged = false;
                authPinGrantPendingLogged = false;
                authCorrectiveActionLogged = false;
                authOverlayStateKnown = false;
                authOverlayVisible = false;
                authOverlayExclusiveInput = false;
                authOverlayTransitionCount = 0;
                authOverlayBounceFailed = false;

                RTB_INFO(GetAuthAttemptPrefix() + " started." + BuildAuthAttemptDetails());
            }

            void EndAuthLoginAttempt(const std::string& finalState = "Finished")
            {
                if (authLoginInFlight) {
                    RTB_INFO(GetAuthAttemptPrefix() + " finished. State: " + finalState +
                        " ElapsedMs: " + std::to_string(GetAuthAttemptElapsedMilliseconds()) +
                        " OverlayTransitions: " + std::to_string(authOverlayTransitionCount));
                }

                authLoginInFlight = false;
                authPinGrantLogged = false;
                authPinGrantPendingLogged = false;
                authCorrectiveActionLogged = false;
                authAttemptStartMilliseconds = 0;
                authOverlayStateKnown = false;
                authOverlayVisible = false;
                authOverlayExclusiveInput = false;
                authOverlayTransitionCount = 0;
                authOverlayBounceFailed = false;
            }

            std::string GetAuthAttemptPrefix() const
            {
                return "OnlineIdentity: Auth attempt #" + std::to_string(authAttemptId);
            }

            std::string BuildAuthAttemptDetails() const
            {
                std::string details = " Type: ";
                details += ToString(activeLoginType);
                details += " OverlayEnabled: ";
                details += (authOverlayDiagnosticsEnabled ? "true" : "false");

                if (activeLoginType == OnlineLoginType::DeveloperAuth) {
                    details += " Host: ";
                    details += developerAuthHost.empty() ? "<empty>" : developerAuthHost;
                    details += " Credential: ";
                    details += developerAuthCredentialName.empty() ? "<empty>" : developerAuthCredentialName;
                }

                return details;
            }

            long long GetAuthAttemptElapsedMilliseconds() const
            {
                if (authAttemptStartMilliseconds <= 0) {
                    return 0;
                }

                return GetSteadyClockMilliseconds() - authAttemptStartMilliseconds;
            }

            void AddAuthLoginStatusNotification()
            {
                if (!authHandle || authLoginStatusChangedNotificationId != EOS_INVALID_NOTIFICATIONID) {
                    return;
                }

                EOS_Auth_AddNotifyLoginStatusChangedOptions options{};
                options.ApiVersion = EOS_AUTH_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST;
                authLoginStatusChangedNotificationId =
                    EOS_Auth_AddNotifyLoginStatusChanged(authHandle, &options, this,
                        &Impl::OnAuthLoginStatusChanged);

                if (authLoginStatusChangedNotificationId == EOS_INVALID_NOTIFICATIONID) {
                    RTB_WARN("OnlineIdentity: EOS_Auth_AddNotifyLoginStatusChanged returned an invalid notification id.");
                } else {
                    RTB_INFO("OnlineIdentity: EOS Auth login status diagnostics registered.");
                }
            }

            void RemoveAuthLoginStatusNotification()
            {
                if (!authHandle || authLoginStatusChangedNotificationId == EOS_INVALID_NOTIFICATIONID) {
                    authLoginStatusChangedNotificationId = EOS_INVALID_NOTIFICATIONID;
                    return;
                }

                EOS_Auth_RemoveNotifyLoginStatusChanged(authHandle, authLoginStatusChangedNotificationId);
                authLoginStatusChangedNotificationId = EOS_INVALID_NOTIFICATIONID;
            }

            void LogAuthAccounts(const std::string& context) const
            {
                if (!authHandle) {
                    return;
                }

                const int32_t accountCount = EOS_Auth_GetLoggedInAccountsCount(authHandle);
                RTB_INFO("OnlineIdentity: EOS Auth accounts after " + context + ": " +
                    std::to_string(accountCount));

                for (int32_t index = 0; index < accountCount; ++index) {
                    EOS_EpicAccountId accountId =
                        EOS_Auth_GetLoggedInAccountByIndex(authHandle, index);
                    const std::string accountIdText = EpicAccountIdToString(accountId);
                    const EOS_ELoginStatus loginStatus =
                        EOS_Auth_GetLoginStatus(authHandle, accountId);

                    RTB_INFO("OnlineIdentity: EOS Auth account[" + std::to_string(index) +
                        "] Id: " + (accountIdText.empty() ? "Invalid" : accountIdText) +
                        " Status: " + AuthLoginStatusToString(loginStatus));
                }
            }

            bool CopyEpicIdToken(EOS_EpicAccountId epicAccountId, std::string& outToken)
            {
                // Preferred bridge: Auth Epic account -> Connect Product User via ID token.
                EOS_Auth_CopyIdTokenOptions options{};
                options.ApiVersion = EOS_AUTH_COPYIDTOKEN_API_LATEST;
                options.AccountId = epicAccountId;

                EOS_Auth_IdToken* idToken = nullptr;
                const EOS_EResult result = EOS_Auth_CopyIdToken(authHandle, &options, &idToken);
                if (result != EOS_EResult::EOS_Success || !idToken || !idToken->JsonWebToken) {
                    if (idToken) {
                        EOS_Auth_IdToken_Release(idToken);
                    }
                    return false;
                }

                outToken = idToken->JsonWebToken;
                EOS_Auth_IdToken_Release(idToken);
                return !outToken.empty();
            }

            bool CopyEpicAccessToken(EOS_EpicAccountId epicAccountId, std::string& outToken)
            {
                // Backwards-compatible fallback accepted by Connect as EOS_ECT_EPIC.
                EOS_Auth_CopyUserAuthTokenOptions options{};
                options.ApiVersion = EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST;

                EOS_Auth_Token* authToken = nullptr;
                const EOS_EResult result = EOS_Auth_CopyUserAuthToken(authHandle, &options, epicAccountId, &authToken);
                if (result != EOS_EResult::EOS_Success || !authToken || !authToken->AccessToken) {
                    if (authToken) {
                        EOS_Auth_Token_Release(authToken);
                    }
                    return false;
                }

                outToken = authToken->AccessToken;
                EOS_Auth_Token_Release(authToken);
                return !outToken.empty();
            }

            void StartEpicConnectLogin(EOS_EpicAccountId epicAccountId)
            {
                connectLoginToken.clear();

                EOS_EExternalCredentialType credentialType = EOS_EExternalCredentialType::EOS_ECT_EPIC_ID_TOKEN;
                if (!CopyEpicIdToken(epicAccountId, connectLoginToken)) {
                    RTB_WARN("OnlineIdentity: EOS_Auth_CopyIdToken failed. Falling back to Auth access token.");
                    credentialType = EOS_EExternalCredentialType::EOS_ECT_EPIC;
                    if (!CopyEpicAccessToken(epicAccountId, connectLoginToken)) {
                        FailLogin("EOS Auth login succeeded, but no token could be copied for Connect login.",
                            OnlineErrorCode::BackendError);
                        return;
                    }
                }

                // Connect is the identity used by Lobbies and P2P, so Auth must hand off to Connect.
                EOS_Connect_Credentials credentials{};
                credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
                credentials.Token = connectLoginToken.c_str();
                credentials.Type = credentialType;

                EOS_Connect_LoginOptions options{};
                options.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
                options.Credentials = &credentials;
                options.UserLoginInfo = nullptr;

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

                RTB_INFO(std::string("OnlineIdentity: EOS ") + ToString(activeLoginType) +
                    " login completed. ProductUserId: " + productUserIdText);
            }

            void FailLogin(const std::string& message, OnlineErrorCode)
            {
                // Move identity into an error state and clear any partial user data.
                EndAuthLoginAttempt("Error: " + message);
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

            static void EOS_CALL OnAuthLoginCompleted(const EOS_Auth_LoginCallbackInfo* data)
            {
                // EOS returns our Impl pointer through ClientData.
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                const std::string resultText = EosResultToString(data->ResultCode);

                if (!self->authLoginInFlight || self->status != OnlineLoginStatus::LoggingIn) {
                    RTB_WARN(self->GetAuthAttemptPrefix() +
                        " ignored stale EOS Auth login callback. Result: " + resultText);
                    self->LogAuthAccounts("stale Auth login callback " + resultText);
                    return;
                }

                RTB_INFO(self->GetAuthAttemptPrefix() + " callback. Result: " +
                    resultText + self->BuildAuthAttemptDetails());
                self->LogAuthAccounts(self->GetAuthAttemptPrefix() + " callback " + resultText);

                if (data->ResultCode == EOS_EResult::EOS_Success) {
                    self->EndAuthLoginAttempt("AuthSuccess");
                    self->localEpicAccountId = data->SelectedAccountId
                        ? data->SelectedAccountId
                        : data->LocalUserId;

                    if (!self->localEpicAccountId) {
                        self->FailLogin("EOS Auth returned an invalid Epic Account ID.", OnlineErrorCode::BackendError);
                        return;
                    }

                    self->LogAuthAccounts("successful Auth login");
                    self->StartEpicConnectLogin(self->localEpicAccountId);
                    return;
                }

                if (data->ResultCode == EOS_EResult::EOS_Auth_CorrectiveActionRequired) {
                    if (!self->authCorrectiveActionLogged) {
                        self->authCorrectiveActionLogged = true;
                        RTB_WARN(BuildIntermediateAuthMessage(self->activeLoginType, data->ResultCode));
                    }
                    return;
                }

                if (IsFinalAuthFailureResult(data->ResultCode)) {
                    self->FailLogin(
                        BuildAuthFailureMessage(
                            self->activeLoginType,
                            data->ResultCode,
                            self->developerAuthCredentialName,
                            self->developerAuthHost),
                        OnlineErrorCode::BackendError
                    );
                    return;
                }

                if (data->ResultCode == EOS_EResult::EOS_Auth_PinGrantCode) {
                    if (!self->authPinGrantLogged) {
                        self->authPinGrantLogged = true;
                        RTB_WARN(BuildPinGrantMessage(data->PinGrantInfo));
                    }
                    return;
                }

                if (data->ResultCode == EOS_EResult::EOS_Auth_PinGrantPending) {
                    if (!self->authPinGrantPendingLogged) {
                        self->authPinGrantPendingLogged = true;
                        RTB_WARN(BuildIntermediateAuthMessage(self->activeLoginType, data->ResultCode));
                    }
                    return;
                }

                if (EOS_EResult_IsOperationComplete(data->ResultCode)) {
                    self->FailLogin(
                        BuildAuthFailureMessage(
                            self->activeLoginType,
                            data->ResultCode,
                            self->developerAuthCredentialName,
                            self->developerAuthHost),
                        OnlineErrorCode::BackendError
                    );
                    return;
                }

                RTB_WARN(BuildIntermediateAuthMessage(self->activeLoginType, data->ResultCode));
            }

            static void EOS_CALL OnAuthLoginStatusChanged(
                const EOS_Auth_LoginStatusChangedCallbackInfo* data)
            {
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                const std::string accountIdText = EpicAccountIdToString(data->LocalUserId);
                RTB_INFO("OnlineIdentity: EOS Auth login status changed. AccountId: " +
                    (accountIdText.empty() ? std::string("Invalid") : accountIdText) +
                    " Previous: " + AuthLoginStatusToString(data->PrevStatus) +
                    " Current: " + AuthLoginStatusToString(data->CurrentStatus));
                self->LogAuthAccounts("Auth login status change");
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

            static void EOS_CALL OnAuthLogoutCompleted(const EOS_Auth_LogoutCallbackInfo* data)
            {
                // Logout is best-effort because engine state has already been cleared.
                if (!data) {
                    return;
                }

                if (data->ResultCode != EOS_EResult::EOS_Success) {
                    RTB_WARN("OnlineIdentity: EOS_Auth_Logout returned " + EosResultToString(data->ResultCode));
                }
            }

            void* platformHandle = nullptr;
            EOS_HAuth authHandle = nullptr;
            EOS_HConnect connectHandle = nullptr;
            EOS_NotificationId authLoginStatusChangedNotificationId = EOS_INVALID_NOTIFICATIONID;
            EOS_EpicAccountId localEpicAccountId = nullptr;
            EOS_ProductUserId localProductUserId = nullptr;
            OnlineLoginStatus status = OnlineLoginStatus::NotLoggedIn;
            OnlineLoginType activeLoginType = OnlineLoginType::DeviceId;
            bool authLoginInFlight = false;
            bool authOverlayDiagnosticsEnabled = false;
            bool authPinGrantLogged = false;
            bool authPinGrantPendingLogged = false;
            bool authCorrectiveActionLogged = false;
            bool authOverlayStateKnown = false;
            bool authOverlayVisible = false;
            bool authOverlayExclusiveInput = false;
            bool authOverlayBounceFailed = false;
            int authAttemptId = 0;
            int authOverlayTransitionCount = 0;
            long long authAttemptStartMilliseconds = 0;
            OnlineUserId localUserId;
            std::string displayName;
            std::string lastError;
            std::string developerAuthHost;
            std::string developerAuthCredentialName;
            std::string connectLoginToken;
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

        void EosOnlineIdentity::SetAuthOverlayDiagnosticsEnabled(bool enabled)
        {
            impl->SetAuthOverlayDiagnosticsEnabled(enabled);
        }

        bool EosOnlineIdentity::NotifyAuthOverlayDisplayState(bool visible, bool exclusiveInput)
        {
            return impl->NotifyAuthOverlayDisplayState(visible, exclusiveInput);
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
