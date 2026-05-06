#include "EosOnlineLobby.h"

#include "IOnlineIdentity.h"
#include "../Core/Logger.h"

#include <eos_common.h>
#include <eos_lobby.h>
#include <eos_lobby_types.h>
#include <eos_sdk.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace {

    std::string EosResultToString(EOS_EResult result)
    {
        const char* resultText = EOS_EResult_ToString(result);
        return resultText ? resultText : "EOS_Unknown";
    }

}

namespace RTBEngine {
    namespace Online {

        class EosOnlineLobby::Impl {
        public:
            explicit Impl(IOnlineIdentity* identity)
                : identity(identity)
            {
            }

            void SetPlatformHandle(void* handle)
            {
                // Store the platform and resolve the EOS Lobby interface from it.
                platformHandle = handle;
                lobbyHandle = platformHandle
                    ? EOS_Platform_GetLobbyInterface(static_cast<EOS_HPlatform>(platformHandle))
                    : nullptr;
            }

            void ResetPlatformHandle()
            {
                // Forget EOS handles before the backend releases the platform.
                platformHandle = nullptr;
                lobbyHandle = nullptr;
                currentLobby = {};
                lastError.clear();
                SetState(OnlineLobbyState::NotInLobby);
            }

            OnlineResult CreateLobby(const OnlineCreateLobbyOptions& options)
            {
                if (!lobbyHandle) {
                    FailCreate("EOS Lobby interface is not available.", OnlineErrorCode::InvalidState);
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                if (!identity || !identity->IsLoggedIn()) {
                    FailCreate("Cannot create an EOS lobby without a logged-in local identity.", OnlineErrorCode::InvalidState);
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                if (state == OnlineLobbyState::Creating || state == OnlineLobbyState::Destroying) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
                }

                if (state == OnlineLobbyState::InLobby && !currentLobby.lobbyId.empty()) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Already in a lobby.");
                }

                EOS_ProductUserId localProductUserId = ResolveLocalProductUserId();
                if (!localProductUserId) {
                    FailCreate("Local identity does not contain a valid EOS Product User ID.", OnlineErrorCode::InvalidState);
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                pendingCreateOptions = options;
                pendingCreateOptions.maxMembers = std::max<std::uint32_t>(2, pendingCreateOptions.maxMembers);
                if (pendingCreateOptions.bucketId.empty()) {
                    pendingCreateOptions.bucketId = "RTBEngine";
                }

                currentLobby = {};
                lastError.clear();
                SetState(OnlineLobbyState::Creating);

                // Ask EOS to create an advertised lobby owned by the local Product User.
                EOS_Lobby_CreateLobbyOptions eosOptions{};
                eosOptions.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
                eosOptions.LocalUserId = localProductUserId;
                eosOptions.MaxLobbyMembers = pendingCreateOptions.maxMembers;
                eosOptions.PermissionLevel = pendingCreateOptions.publicAdvertised
                    ? EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED
                    : EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY;
                eosOptions.bPresenceEnabled = EOS_FALSE;
                eosOptions.bAllowInvites = pendingCreateOptions.allowInvites ? EOS_TRUE : EOS_FALSE;
                eosOptions.BucketId = pendingCreateOptions.bucketId.c_str();
                eosOptions.bDisableHostMigration = pendingCreateOptions.allowHostMigration ? EOS_FALSE : EOS_TRUE;
                eosOptions.bEnableRTCRoom = pendingCreateOptions.enableRtcRoom ? EOS_TRUE : EOS_FALSE;
                eosOptions.LocalRTCOptions = nullptr;
                eosOptions.LobbyId = nullptr;
                eosOptions.bEnableJoinById = pendingCreateOptions.allowJoinById ? EOS_TRUE : EOS_FALSE;
                eosOptions.bRejoinAfterKickRequiresInvite = EOS_TRUE;
                eosOptions.AllowedPlatformIds = nullptr;
                eosOptions.AllowedPlatformIdsCount = 0;
                eosOptions.bCrossplayOptOut = EOS_FALSE;
                eosOptions.RTCRoomJoinActionType = EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_ManualJoin;

                EOS_Lobby_CreateLobby(lobbyHandle, &eosOptions, this, &Impl::OnCreateLobbyCompleted);
                return OnlineResult::Success("EOS lobby creation started.");
            }

            OnlineResult DestroyLobby()
            {
                if (!lobbyHandle) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "EOS Lobby interface is not available.");
                }

                if (state == OnlineLobbyState::Creating || state == OnlineLobbyState::Destroying) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
                }

                if (currentLobby.lobbyId.empty()) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "No active lobby to destroy.");
                }

                EOS_ProductUserId localProductUserId = ResolveLocalProductUserId();
                if (!localProductUserId) {
                    lastError = "Local identity does not contain a valid EOS Product User ID.";
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                lastError.clear();
                SetState(OnlineLobbyState::Destroying);

                // Lobby owner destruction closes the lobby for every member.
                EOS_Lobby_DestroyLobbyOptions options{};
                options.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
                options.LocalUserId = localProductUserId;
                options.LobbyId = currentLobby.lobbyId.c_str();

                EOS_Lobby_DestroyLobby(lobbyHandle, &options, this, &Impl::OnDestroyLobbyCompleted);
                return OnlineResult::Success("EOS lobby destruction started.");
            }

            OnlineLobbyState GetState() const
            {
                return state;
            }

            const OnlineLobbyInfo& GetCurrentLobby() const
            {
                return currentLobby;
            }

            const char* GetLastError() const
            {
                return lastError.c_str();
            }

            Core::EventSubscription SubscribeLobbyStatusChanged(Core::Event<OnlineLobbyStatusChangedEvent>::Callback callback)
            {
                return lobbyStatusChanged.Subscribe(std::move(callback));
            }

            void ClearLobbyStatusChangedListeners()
            {
                lobbyStatusChanged.Clear();
            }

        private:
            EOS_ProductUserId ResolveLocalProductUserId() const
            {
                if (!identity) {
                    return nullptr;
                }

                const OnlineUserId& localUserId = identity->GetLocalUserId();
                if (localUserId.GetType() != OnlineUserIdType::EOSProductUser || localUserId.GetValue().empty()) {
                    return nullptr;
                }

                EOS_ProductUserId productUserId = EOS_ProductUserId_FromString(localUserId.GetValue().c_str());
                return EOS_ProductUserId_IsValid(productUserId) == EOS_TRUE ? productUserId : nullptr;
            }

            void CompleteCreate(const char* lobbyId)
            {
                currentLobby = {};
                currentLobby.lobbyId = lobbyId ? lobbyId : "";
                currentLobby.ownerUserId = identity ? identity->GetLocalUserId() : OnlineUserId();
                currentLobby.maxMembers = pendingCreateOptions.maxMembers;
                currentLobby.availableSlots = pendingCreateOptions.maxMembers > 0 ? pendingCreateOptions.maxMembers - 1 : 0;
                currentLobby.isOwner = true;
                lastError.clear();
                SetState(OnlineLobbyState::InLobby);

                RTB_INFO("OnlineLobby: EOS lobby created. LobbyId: " + currentLobby.lobbyId);
            }

            void CompleteDestroy()
            {
                const std::string destroyedLobbyId = currentLobby.lobbyId;
                currentLobby = {};
                lastError.clear();
                SetState(OnlineLobbyState::NotInLobby);

                RTB_INFO("OnlineLobby: EOS lobby destroyed. LobbyId: " + destroyedLobbyId);
            }

            void FailCreate(const std::string& message, OnlineErrorCode)
            {
                currentLobby = {};
                lastError = message;
                SetState(OnlineLobbyState::Error);
                RTB_ERROR("OnlineLobby: " + lastError);
            }

            void FailDestroy(const std::string& message)
            {
                lastError = message;
                SetState(OnlineLobbyState::InLobby);
                RTB_ERROR("OnlineLobby: " + lastError);
            }

            void SetState(OnlineLobbyState newState)
            {
                const OnlineLobbyState previousState = state;
                state = newState;

                if (previousState == newState) {
                    return;
                }

                lobbyStatusChanged.Invoke({ previousState, state, currentLobby });
            }

            static void EOS_CALL OnCreateLobbyCompleted(const EOS_Lobby_CreateLobbyCallbackInfo* data)
            {
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                if (data->ResultCode == EOS_EResult::EOS_Success) {
                    self->CompleteCreate(data->LobbyId);
                    return;
                }

                self->FailCreate(
                    "EOS_Lobby_CreateLobby failed: " + EosResultToString(data->ResultCode),
                    OnlineErrorCode::BackendError
                );
            }

            static void EOS_CALL OnDestroyLobbyCompleted(const EOS_Lobby_DestroyLobbyCallbackInfo* data)
            {
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                if (data->ResultCode == EOS_EResult::EOS_Success) {
                    self->CompleteDestroy();
                    return;
                }

                self->FailDestroy("EOS_Lobby_DestroyLobby failed: " + EosResultToString(data->ResultCode));
            }

            IOnlineIdentity* identity = nullptr;
            void* platformHandle = nullptr;
            EOS_HLobby lobbyHandle = nullptr;
            OnlineLobbyState state = OnlineLobbyState::NotInLobby;
            OnlineLobbyInfo currentLobby;
            OnlineCreateLobbyOptions pendingCreateOptions;
            std::string lastError;
            Core::Event<OnlineLobbyStatusChangedEvent> lobbyStatusChanged;
        };

        EosOnlineLobby::EosOnlineLobby(IOnlineIdentity* identity)
            : impl(std::make_unique<Impl>(identity))
        {
        }

        EosOnlineLobby::~EosOnlineLobby() = default;

        void EosOnlineLobby::SetPlatformHandle(void* handle)
        {
            impl->SetPlatformHandle(handle);
        }

        void EosOnlineLobby::ResetPlatformHandle()
        {
            impl->ResetPlatformHandle();
        }

        OnlineResult EosOnlineLobby::CreateLobby(const OnlineCreateLobbyOptions& options)
        {
            return impl->CreateLobby(options);
        }

        OnlineResult EosOnlineLobby::DestroyLobby()
        {
            return impl->DestroyLobby();
        }

        OnlineLobbyState EosOnlineLobby::GetState() const
        {
            return impl->GetState();
        }

        const OnlineLobbyInfo& EosOnlineLobby::GetCurrentLobby() const
        {
            return impl->GetCurrentLobby();
        }

        const char* EosOnlineLobby::GetLastError() const
        {
            return impl->GetLastError();
        }

        Core::EventSubscription EosOnlineLobby::SubscribeLobbyStatusChanged(Core::Event<OnlineLobbyStatusChangedEvent>::Callback callback)
        {
            return impl->SubscribeLobbyStatusChanged(std::move(callback));
        }

        void EosOnlineLobby::ClearLobbyStatusChangedListeners()
        {
            impl->ClearLobbyStatusChangedListeners();
        }

    }
}
