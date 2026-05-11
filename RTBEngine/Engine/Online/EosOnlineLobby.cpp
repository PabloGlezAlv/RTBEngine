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
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

    std::string EosResultToString(EOS_EResult result)
    {
        const char* resultText = EOS_EResult_ToString(result);
        return resultText ? resultText : "EOS_Unknown";
    }

    std::string ProductUserIdToString(EOS_ProductUserId productUserId)
    {
        char buffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1]{};
        int32_t bufferLength = static_cast<int32_t>(sizeof(buffer));

        const EOS_EResult result = EOS_ProductUserId_ToString(productUserId, buffer, &bufferLength);
        if (result != EOS_EResult::EOS_Success) {
            return {};
        }

        return buffer;
    }

    bool IsOperationInProgress(RTBEngine::Online::OnlineLobbyState state)
    {
        return state == RTBEngine::Online::OnlineLobbyState::Creating ||
            state == RTBEngine::Online::OnlineLobbyState::Searching ||
            state == RTBEngine::Online::OnlineLobbyState::Joining ||
            state == RTBEngine::Online::OnlineLobbyState::Leaving ||
            state == RTBEngine::Online::OnlineLobbyState::Destroying;
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
                UnregisterLobbyNotifications();
                platformHandle = handle;
                lobbyHandle = platformHandle
                    ? EOS_Platform_GetLobbyInterface(static_cast<EOS_HPlatform>(platformHandle))
                    : nullptr;
                RegisterLobbyNotifications();
            }

            void ResetPlatformHandle()
            {
                // Release search/detail handles before the platform that owns them goes away.
                UnregisterLobbyNotifications();
                ReleaseSearchHandle();
                ReleasePendingJoinDetails();

                platformHandle = nullptr;
                lobbyHandle = nullptr;
                currentLobby = {};
                searchResults.clear();
                pendingCreateOptions = {};
                pendingJoinInfo = {};
                pendingJoinLobbyId.clear();
                lastError.clear();
                SetState(OnlineLobbyState::NotInLobby);
            }

            void Tick(float deltaTime)
            {
                if (state != OnlineLobbyState::InLobby || currentLobby.lobbyId.empty()) {
                    lobbyRefreshTimer = 0.0f;
                    return;
                }

                lobbyRefreshTimer += deltaTime;
                if (lobbyRefreshTimer < 1.0f) {
                    return;
                }

                lobbyRefreshTimer = 0.0f;
                RefreshCurrentLobbyFromDetails();
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

                if (IsOperationInProgress(state)) {
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
                searchResults.clear();
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

            OnlineResult FindLobbies(const OnlineFindLobbiesOptions& options)
            {
                if (!lobbyHandle) {
                    FailFind("EOS Lobby interface is not available.");
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                if (!identity || !identity->IsLoggedIn()) {
                    FailFind("Cannot search EOS lobbies without a logged-in local identity.");
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                if (IsOperationInProgress(state)) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
                }

                if (state == OnlineLobbyState::InLobby && !currentLobby.lobbyId.empty()) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Leave the current lobby before searching.");
                }

                if (options.lobbyId.empty()) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, "Lobby Id is required.");
                }

                EOS_ProductUserId localProductUserId = ResolveLocalProductUserId();
                if (!localProductUserId) {
                    FailFind("Local identity does not contain a valid EOS Product User ID.");
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                ReleaseSearchHandle();
                searchResults.clear();
                lastError.clear();

                const std::uint32_t maxResults = std::clamp<std::uint32_t>(options.maxResults, 1, 50);
                EOS_Lobby_CreateLobbySearchOptions createSearchOptions{};
                createSearchOptions.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
                createSearchOptions.MaxResults = maxResults;

                EOS_HLobbySearch newSearchHandle = nullptr;
                EOS_EResult result = EOS_Lobby_CreateLobbySearch(lobbyHandle, &createSearchOptions, &newSearchHandle);
                if (result != EOS_EResult::EOS_Success || !newSearchHandle) {
                    FailFind("EOS_Lobby_CreateLobbySearch failed: " + EosResultToString(result));
                    return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
                }

                activeSearchHandle = newSearchHandle;

                // Search by exact LobbyId for this first debuggable lobby flow.
                EOS_LobbySearch_SetLobbyIdOptions setLobbyIdOptions{};
                setLobbyIdOptions.ApiVersion = EOS_LOBBYSEARCH_SETLOBBYID_API_LATEST;
                setLobbyIdOptions.LobbyId = options.lobbyId.c_str();

                result = EOS_LobbySearch_SetLobbyId(activeSearchHandle, &setLobbyIdOptions);
                if (result != EOS_EResult::EOS_Success) {
                    FailFind("EOS_LobbySearch_SetLobbyId failed: " + EosResultToString(result));
                    return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
                }

                SetState(OnlineLobbyState::Searching);

                EOS_LobbySearch_FindOptions findOptions{};
                findOptions.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST;
                findOptions.LocalUserId = localProductUserId;

                EOS_LobbySearch_Find(activeSearchHandle, &findOptions, this, &Impl::OnFindLobbiesCompleted);
                return OnlineResult::Success("EOS lobby search started.");
            }

            OnlineResult JoinLobby(const OnlineJoinLobbyOptions& options)
            {
                if (!lobbyHandle) {
                    FailJoin("EOS Lobby interface is not available.");
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                if (!identity || !identity->IsLoggedIn()) {
                    FailJoin("Cannot join an EOS lobby without a logged-in local identity.");
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                if (IsOperationInProgress(state)) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
                }

                if (state == OnlineLobbyState::InLobby && !currentLobby.lobbyId.empty()) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Already in a lobby.");
                }

                if (options.lobbyId.empty()) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, "Lobby Id is required.");
                }

                EOS_ProductUserId localProductUserId = ResolveLocalProductUserId();
                if (!localProductUserId) {
                    FailJoin("Local identity does not contain a valid EOS Product User ID.");
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                ReleaseSearchHandle();
                ReleasePendingJoinDetails();
                pendingJoinInfo = {};
                pendingJoinLobbyId = options.lobbyId;
                lastError.clear();

                EOS_Lobby_CreateLobbySearchOptions createSearchOptions{};
                createSearchOptions.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
                createSearchOptions.MaxResults = 1;

                EOS_HLobbySearch newSearchHandle = nullptr;
                EOS_EResult result = EOS_Lobby_CreateLobbySearch(lobbyHandle, &createSearchOptions, &newSearchHandle);
                if (result != EOS_EResult::EOS_Success || !newSearchHandle) {
                    FailJoin("EOS_Lobby_CreateLobbySearch failed: " + EosResultToString(result));
                    return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
                }

                activeSearchHandle = newSearchHandle;

                // Join needs a LobbyDetails handle, so we search the exact id first.
                EOS_LobbySearch_SetLobbyIdOptions setLobbyIdOptions{};
                setLobbyIdOptions.ApiVersion = EOS_LOBBYSEARCH_SETLOBBYID_API_LATEST;
                setLobbyIdOptions.LobbyId = pendingJoinLobbyId.c_str();

                result = EOS_LobbySearch_SetLobbyId(activeSearchHandle, &setLobbyIdOptions);
                if (result != EOS_EResult::EOS_Success) {
                    FailJoin("EOS_LobbySearch_SetLobbyId failed: " + EosResultToString(result));
                    return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
                }

                SetState(OnlineLobbyState::Joining);

                EOS_LobbySearch_FindOptions findOptions{};
                findOptions.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST;
                findOptions.LocalUserId = localProductUserId;

                EOS_LobbySearch_Find(activeSearchHandle, &findOptions, this, &Impl::OnJoinSearchCompleted);
                return OnlineResult::Success("EOS lobby join started.");
            }

            OnlineResult LeaveLobby()
            {
                if (!lobbyHandle) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "EOS Lobby interface is not available.");
                }

                if (IsOperationInProgress(state)) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
                }

                if (currentLobby.lobbyId.empty()) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "No active lobby to leave.");
                }

                EOS_ProductUserId localProductUserId = ResolveLocalProductUserId();
                if (!localProductUserId) {
                    lastError = "Local identity does not contain a valid EOS Product User ID.";
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
                }

                lastError.clear();
                SetState(OnlineLobbyState::Leaving);

                // Members leave the lobby without destroying it for the rest of the room.
                EOS_Lobby_LeaveLobbyOptions options{};
                options.ApiVersion = EOS_LOBBY_LEAVELOBBY_API_LATEST;
                options.LocalUserId = localProductUserId;
                options.LobbyId = currentLobby.lobbyId.c_str();

                EOS_Lobby_LeaveLobby(lobbyHandle, &options, this, &Impl::OnLeaveLobbyCompleted);
                return OnlineResult::Success("EOS lobby leave started.");
            }

            OnlineResult DestroyLobby()
            {
                if (!lobbyHandle) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "EOS Lobby interface is not available.");
                }

                if (IsOperationInProgress(state)) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
                }

                if (currentLobby.lobbyId.empty()) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "No active lobby to destroy.");
                }

                if (!currentLobby.isOwner) {
                    return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Only the lobby owner can destroy the lobby. Use LeaveLobby instead.");
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

            const std::vector<OnlineLobbyInfo>& GetSearchResults() const
            {
                return searchResults;
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

            OnlineLobbyInfo BuildLobbyInfoFromDetails(EOS_HLobbyDetails detailsHandle) const
            {
                OnlineLobbyInfo lobbyInfo;
                if (!detailsHandle) {
                    return lobbyInfo;
                }

                EOS_LobbyDetails_CopyInfoOptions copyInfoOptions{};
                copyInfoOptions.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;

                EOS_LobbyDetails_Info* eosInfo = nullptr;
                const EOS_EResult result = EOS_LobbyDetails_CopyInfo(detailsHandle, &copyInfoOptions, &eosInfo);
                if (result != EOS_EResult::EOS_Success || !eosInfo) {
                    return lobbyInfo;
                }

                // Copy EOS-owned lobby data into provider-neutral engine data.
                lobbyInfo.lobbyId = eosInfo->LobbyId ? eosInfo->LobbyId : "";
                const std::string ownerId = ProductUserIdToString(eosInfo->LobbyOwnerUserId);
                if (!ownerId.empty()) {
                    lobbyInfo.ownerUserId = OnlineUserId(OnlineUserIdType::EOSProductUser, ownerId);
                }
                lobbyInfo.maxMembers = eosInfo->MaxMembers;
                lobbyInfo.availableSlots = eosInfo->AvailableSlots;
                const std::uint32_t slotDerivedMembers = lobbyInfo.maxMembers >= lobbyInfo.availableSlots
                    ? lobbyInfo.maxMembers - lobbyInfo.availableSlots
                    : 0;
                lobbyInfo.currentMembers = slotDerivedMembers;
                lobbyInfo.isOwner = identity && identity->GetLocalUserId() == lobbyInfo.ownerUserId;

                EOS_LobbyDetails_Info_Release(eosInfo);

                EOS_LobbyDetails_GetMemberCountOptions memberCountOptions{};
                memberCountOptions.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST;
                const std::uint32_t memberCount = EOS_LobbyDetails_GetMemberCount(detailsHandle, &memberCountOptions);
                if (memberCount > lobbyInfo.currentMembers) {
                    lobbyInfo.currentMembers = memberCount;
                    lobbyInfo.availableSlots = lobbyInfo.maxMembers > memberCount
                        ? lobbyInfo.maxMembers - memberCount
                        : 0;
                }

                return lobbyInfo;
            }

            void CopySearchResultsFromActiveSearch()
            {
                searchResults.clear();
                if (!activeSearchHandle) {
                    return;
                }

                EOS_LobbySearch_GetSearchResultCountOptions countOptions{};
                countOptions.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
                const std::uint32_t resultCount = EOS_LobbySearch_GetSearchResultCount(activeSearchHandle, &countOptions);

                for (std::uint32_t index = 0; index < resultCount; ++index) {
                    EOS_LobbySearch_CopySearchResultByIndexOptions copyOptions{};
                    copyOptions.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
                    copyOptions.LobbyIndex = index;

                    EOS_HLobbyDetails detailsHandle = nullptr;
                    const EOS_EResult result = EOS_LobbySearch_CopySearchResultByIndex(activeSearchHandle, &copyOptions, &detailsHandle);
                    if (result != EOS_EResult::EOS_Success || !detailsHandle) {
                        continue;
                    }

                    OnlineLobbyInfo lobbyInfo = BuildLobbyInfoFromDetails(detailsHandle);
                    if (!lobbyInfo.lobbyId.empty()) {
                        searchResults.push_back(std::move(lobbyInfo));
                    }

                    EOS_LobbyDetails_Release(detailsHandle);
                }
            }

            bool CopyFirstSearchResultForJoin()
            {
                if (!activeSearchHandle) {
                    return false;
                }

                EOS_LobbySearch_GetSearchResultCountOptions countOptions{};
                countOptions.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
                const std::uint32_t resultCount = EOS_LobbySearch_GetSearchResultCount(activeSearchHandle, &countOptions);
                if (resultCount == 0) {
                    return false;
                }

                EOS_LobbySearch_CopySearchResultByIndexOptions copyOptions{};
                copyOptions.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
                copyOptions.LobbyIndex = 0;

                ReleasePendingJoinDetails();
                const EOS_EResult result = EOS_LobbySearch_CopySearchResultByIndex(activeSearchHandle, &copyOptions, &pendingJoinDetails);
                if (result != EOS_EResult::EOS_Success || !pendingJoinDetails) {
                    return false;
                }

                pendingJoinInfo = BuildLobbyInfoFromDetails(pendingJoinDetails);
                if (pendingJoinInfo.lobbyId.empty()) {
                    pendingJoinInfo.lobbyId = pendingJoinLobbyId;
                }

                return true;
            }

            void RefreshCurrentLobbyFromDetails()
            {
                if (!lobbyHandle || currentLobby.lobbyId.empty()) {
                    return;
                }

                const std::uint32_t previousMemberCount = currentLobby.currentMembers;

                EOS_ProductUserId localProductUserId = ResolveLocalProductUserId();
                if (!localProductUserId) {
                    return;
                }

                EOS_Lobby_CopyLobbyDetailsHandleOptions options{};
                options.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
                options.LobbyId = currentLobby.lobbyId.c_str();
                options.LocalUserId = localProductUserId;

                EOS_HLobbyDetails detailsHandle = nullptr;
                const EOS_EResult result = EOS_Lobby_CopyLobbyDetailsHandle(lobbyHandle, &options, &detailsHandle);
                if (result != EOS_EResult::EOS_Success || !detailsHandle) {
                    return;
                }

                OnlineLobbyInfo refreshedLobby = BuildLobbyInfoFromDetails(detailsHandle);
                EOS_LobbyDetails_Release(detailsHandle);

                if (refreshedLobby.lobbyId.empty()) {
                    return;
                }

                // EOS member details can lag behind lobby slot data, so never let a passive refresh
                // erase members that were already observed through lobby status notifications.
                if (refreshedLobby.currentMembers < previousMemberCount) {
                    refreshedLobby.currentMembers = previousMemberCount;
                    refreshedLobby.availableSlots = refreshedLobby.maxMembers > refreshedLobby.currentMembers
                        ? refreshedLobby.maxMembers - refreshedLobby.currentMembers
                        : 0;
                }

                currentLobby = std::move(refreshedLobby);
                SeedKnownMembersFromCurrentLobby();
            }

            void CompleteCreate(const char* lobbyId)
            {
                currentLobby = {};
                currentLobby.lobbyId = lobbyId ? lobbyId : "";
                currentLobby.ownerUserId = identity ? identity->GetLocalUserId() : OnlineUserId();
                currentLobby.currentMembers = 1;
                currentLobby.maxMembers = pendingCreateOptions.maxMembers;
                currentLobby.availableSlots = pendingCreateOptions.maxMembers > 0 ? pendingCreateOptions.maxMembers - 1 : 0;
                currentLobby.isOwner = true;
                knownMemberIds.clear();
                TrackKnownMember(currentLobby.ownerUserId);
                lastError.clear();
                SetState(OnlineLobbyState::InLobby);

                RTB_INFO("OnlineLobby: EOS lobby created. LobbyId: " + currentLobby.lobbyId);
            }

            void CompleteFind()
            {
                CopySearchResultsFromActiveSearch();
                ReleaseSearchHandle();
                lastError.clear();
                SetState(OnlineLobbyState::NotInLobby);

                RTB_INFO("OnlineLobby: EOS lobby search completed. Results: " + std::to_string(searchResults.size()));
            }

            void StartJoinFromSearchResult()
            {
                EOS_ProductUserId localProductUserId = ResolveLocalProductUserId();
                if (!localProductUserId) {
                    FailJoin("Local identity does not contain a valid EOS Product User ID.");
                    return;
                }

                if (!CopyFirstSearchResultForJoin()) {
                    FailJoin("No EOS lobby found for LobbyId: " + pendingJoinLobbyId);
                    return;
                }

                ReleaseSearchHandle();

                // EOS joins from the lobby details handle returned by the completed search.
                EOS_Lobby_JoinLobbyOptions joinOptions{};
                joinOptions.ApiVersion = EOS_LOBBY_JOINLOBBY_API_LATEST;
                joinOptions.LobbyDetailsHandle = pendingJoinDetails;
                joinOptions.LocalUserId = localProductUserId;
                joinOptions.bPresenceEnabled = EOS_FALSE;
                joinOptions.LocalRTCOptions = nullptr;
                joinOptions.bCrossplayOptOut = EOS_FALSE;
                joinOptions.RTCRoomJoinActionType = EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_ManualJoin;

                EOS_Lobby_JoinLobby(lobbyHandle, &joinOptions, this, &Impl::OnJoinLobbyCompleted);
            }

            void CompleteJoin(const char* lobbyId)
            {
                currentLobby = pendingJoinInfo;
                if (lobbyId && lobbyId[0] != '\0') {
                    currentLobby.lobbyId = lobbyId;
                }
                if (currentLobby.availableSlots > 0) {
                    --currentLobby.availableSlots;
                }
                currentLobby.currentMembers = currentLobby.maxMembers >= currentLobby.availableSlots
                    ? currentLobby.maxMembers - currentLobby.availableSlots
                    : 1;
                knownMemberIds.clear();
                TrackKnownMember(currentLobby.ownerUserId);
                if (identity) {
                    TrackKnownMember(identity->GetLocalUserId());
                }
                ApplyKnownMemberFloor();

                ReleasePendingJoinDetails();
                pendingJoinInfo = {};
                pendingJoinLobbyId.clear();
                lastError.clear();
                SetState(OnlineLobbyState::InLobby);

                RTB_INFO("OnlineLobby: EOS lobby joined. LobbyId: " + currentLobby.lobbyId);
            }

            void CompleteLeave(const char* lobbyId)
            {
                const std::string leftLobbyId = lobbyId && lobbyId[0] != '\0'
                    ? std::string(lobbyId)
                    : currentLobby.lobbyId;

                currentLobby = {};
                knownMemberIds.clear();
                lastError.clear();
                SetState(OnlineLobbyState::NotInLobby);

                RTB_INFO("OnlineLobby: EOS lobby left. LobbyId: " + leftLobbyId);
            }

            void CompleteDestroy()
            {
                const std::string destroyedLobbyId = currentLobby.lobbyId;
                currentLobby = {};
                knownMemberIds.clear();
                searchResults.clear();
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

            void FailFind(const std::string& message)
            {
                ReleaseSearchHandle();
                searchResults.clear();
                lastError = message;
                SetState(OnlineLobbyState::Error);
                RTB_ERROR("OnlineLobby: " + lastError);
            }

            void FailJoin(const std::string& message)
            {
                ReleaseSearchHandle();
                ReleasePendingJoinDetails();
                currentLobby = {};
                pendingJoinInfo = {};
                pendingJoinLobbyId.clear();
                lastError = message;
                SetState(OnlineLobbyState::Error);
                RTB_ERROR("OnlineLobby: " + lastError);
            }

            void FailLeave(const std::string& message)
            {
                lastError = message;
                SetState(OnlineLobbyState::InLobby);
                RTB_ERROR("OnlineLobby: " + lastError);
            }

            void FailDestroy(const std::string& message)
            {
                lastError = message;
                SetState(OnlineLobbyState::InLobby);
                RTB_ERROR("OnlineLobby: " + lastError);
            }

            void ReleaseSearchHandle()
            {
                if (!activeSearchHandle) {
                    return;
                }

                EOS_LobbySearch_Release(activeSearchHandle);
                activeSearchHandle = nullptr;
            }

            void ReleasePendingJoinDetails()
            {
                if (!pendingJoinDetails) {
                    return;
                }

                EOS_LobbyDetails_Release(pendingJoinDetails);
                pendingJoinDetails = nullptr;
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

            void RegisterLobbyNotifications()
            {
                if (!lobbyHandle) {
                    return;
                }

                EOS_Lobby_AddNotifyLobbyUpdateReceivedOptions lobbyUpdateOptions{};
                lobbyUpdateOptions.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYUPDATERECEIVED_API_LATEST;

                lobbyUpdateNotificationId = EOS_Lobby_AddNotifyLobbyUpdateReceived(
                    lobbyHandle,
                    &lobbyUpdateOptions,
                    this,
                    &Impl::OnLobbyUpdateReceived);

                EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions options{};
                options.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST;

                memberStatusNotificationId = EOS_Lobby_AddNotifyLobbyMemberStatusReceived(
                    lobbyHandle,
                    &options,
                    this,
                    &Impl::OnLobbyMemberStatusReceived);
            }

            void UnregisterLobbyNotifications()
            {
                if (!lobbyHandle) {
                    lobbyUpdateNotificationId = EOS_INVALID_NOTIFICATIONID;
                    memberStatusNotificationId = EOS_INVALID_NOTIFICATIONID;
                    return;
                }

                if (lobbyUpdateNotificationId != EOS_INVALID_NOTIFICATIONID) {
                    EOS_Lobby_RemoveNotifyLobbyUpdateReceived(lobbyHandle, lobbyUpdateNotificationId);
                    lobbyUpdateNotificationId = EOS_INVALID_NOTIFICATIONID;
                }

                if (memberStatusNotificationId != EOS_INVALID_NOTIFICATIONID) {
                    EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived(lobbyHandle, memberStatusNotificationId);
                    memberStatusNotificationId = EOS_INVALID_NOTIFICATIONID;
                }
            }

            void ApplyMemberStatus(const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* data)
            {
                if (!data || currentLobby.lobbyId.empty()) {
                    return;
                }

                const std::string lobbyId = data->LobbyId ? data->LobbyId : "";
                if (lobbyId != currentLobby.lobbyId) {
                    return;
                }

                const OnlineUserId targetUserId(
                    OnlineUserIdType::EOSProductUser,
                    ProductUserIdToString(data->TargetUserId));
                const std::string targetUserText = targetUserId.GetValue();

                switch (data->CurrentStatus) {
                case EOS_ELobbyMemberStatus::EOS_LMS_JOINED:
                    // Count each EOS Product User once; duplicated local Device ID windows are not real EOS members.
                    if (!targetUserText.empty() && knownMemberIds.insert(targetUserText).second) {
                        ApplyKnownMemberFloor();
                    } else if (targetUserText.empty() && currentLobby.currentMembers < currentLobby.maxMembers) {
                        ++currentLobby.currentMembers;
                    }
                    break;
                case EOS_ELobbyMemberStatus::EOS_LMS_LEFT:
                case EOS_ELobbyMemberStatus::EOS_LMS_DISCONNECTED:
                case EOS_ELobbyMemberStatus::EOS_LMS_KICKED:
                    if (!targetUserText.empty() && knownMemberIds.erase(targetUserText) > 0) {
                        currentLobby.currentMembers = static_cast<std::uint32_t>(knownMemberIds.size());
                    } else if (currentLobby.currentMembers > 0) {
                        --currentLobby.currentMembers;
                    }
                    break;
                case EOS_ELobbyMemberStatus::EOS_LMS_CLOSED:
                    currentLobby = {};
                    knownMemberIds.clear();
                    SetState(OnlineLobbyState::NotInLobby);
                    return;
                case EOS_ELobbyMemberStatus::EOS_LMS_PROMOTED:
                    if (identity && targetUserId == identity->GetLocalUserId()) {
                        currentLobby.ownerUserId = identity ? identity->GetLocalUserId() : OnlineUserId();
                        currentLobby.isOwner = true;
                    }
                    break;
                default:
                    break;
                }

                currentLobby.availableSlots = currentLobby.maxMembers > currentLobby.currentMembers
                    ? currentLobby.maxMembers - currentLobby.currentMembers
                    : 0;

                RTB_INFO("OnlineLobby: EOS member status changed. LobbyId: " + currentLobby.lobbyId +
                    " Members: " + std::to_string(currentLobby.currentMembers) + "/" +
                    std::to_string(currentLobby.maxMembers));

                RefreshCurrentLobbyFromDetails();
            }

            void TrackKnownMember(const OnlineUserId& userId)
            {
                if (userId.GetType() == OnlineUserIdType::EOSProductUser && !userId.GetValue().empty()) {
                    knownMemberIds.insert(userId.GetValue());
                }
            }

            void SeedKnownMembersFromCurrentLobby()
            {
                TrackKnownMember(currentLobby.ownerUserId);
                if (identity && identity->IsLoggedIn()) {
                    TrackKnownMember(identity->GetLocalUserId());
                }
                ApplyKnownMemberFloor();
            }

            void ApplyKnownMemberFloor()
            {
                const std::uint32_t knownCount = static_cast<std::uint32_t>(knownMemberIds.size());
                if (knownCount > currentLobby.currentMembers) {
                    currentLobby.currentMembers = knownCount;
                    currentLobby.availableSlots = currentLobby.maxMembers > currentLobby.currentMembers
                        ? currentLobby.maxMembers - currentLobby.currentMembers
                        : 0;
                }
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

            static void EOS_CALL OnFindLobbiesCompleted(const EOS_LobbySearch_FindCallbackInfo* data)
            {
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                if (data->ResultCode == EOS_EResult::EOS_Success) {
                    self->CompleteFind();
                    return;
                }

                self->FailFind("EOS_LobbySearch_Find failed: " + EosResultToString(data->ResultCode));
            }

            static void EOS_CALL OnJoinSearchCompleted(const EOS_LobbySearch_FindCallbackInfo* data)
            {
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                if (data->ResultCode == EOS_EResult::EOS_Success) {
                    self->StartJoinFromSearchResult();
                    return;
                }

                self->FailJoin("EOS_LobbySearch_Find for join failed: " + EosResultToString(data->ResultCode));
            }

            static void EOS_CALL OnJoinLobbyCompleted(const EOS_Lobby_JoinLobbyCallbackInfo* data)
            {
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                if (data->ResultCode == EOS_EResult::EOS_Success) {
                    self->CompleteJoin(data->LobbyId);
                    return;
                }

                self->FailJoin("EOS_Lobby_JoinLobby failed: " + EosResultToString(data->ResultCode));
            }

            static void EOS_CALL OnLeaveLobbyCompleted(const EOS_Lobby_LeaveLobbyCallbackInfo* data)
            {
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                if (data->ResultCode == EOS_EResult::EOS_Success) {
                    self->CompleteLeave(data->LobbyId);
                    return;
                }

                self->FailLeave("EOS_Lobby_LeaveLobby failed: " + EosResultToString(data->ResultCode));
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

            static void EOS_CALL OnLobbyUpdateReceived(const EOS_Lobby_LobbyUpdateReceivedCallbackInfo* data)
            {
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                const std::string lobbyId = data->LobbyId ? data->LobbyId : "";
                if (lobbyId == self->currentLobby.lobbyId) {
                    self->RefreshCurrentLobbyFromDetails();
                }
            }

            static void EOS_CALL OnLobbyMemberStatusReceived(const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* data)
            {
                if (!data || !data->ClientData) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                self->ApplyMemberStatus(data);
            }

            IOnlineIdentity* identity = nullptr;
            void* platformHandle = nullptr;
            EOS_HLobby lobbyHandle = nullptr;
            EOS_HLobbySearch activeSearchHandle = nullptr;
            EOS_HLobbyDetails pendingJoinDetails = nullptr;
            EOS_NotificationId lobbyUpdateNotificationId = EOS_INVALID_NOTIFICATIONID;
            EOS_NotificationId memberStatusNotificationId = EOS_INVALID_NOTIFICATIONID;
            OnlineLobbyState state = OnlineLobbyState::NotInLobby;
            OnlineLobbyInfo currentLobby;
            std::vector<OnlineLobbyInfo> searchResults;
            std::unordered_set<std::string> knownMemberIds;
            OnlineCreateLobbyOptions pendingCreateOptions;
            OnlineLobbyInfo pendingJoinInfo;
            std::string pendingJoinLobbyId;
            std::string lastError;
            float lobbyRefreshTimer = 0.0f;
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

        void EosOnlineLobby::Tick(float deltaTime)
        {
            impl->Tick(deltaTime);
        }

        OnlineResult EosOnlineLobby::CreateLobby(const OnlineCreateLobbyOptions& options)
        {
            return impl->CreateLobby(options);
        }

        OnlineResult EosOnlineLobby::FindLobbies(const OnlineFindLobbiesOptions& options)
        {
            return impl->FindLobbies(options);
        }

        OnlineResult EosOnlineLobby::JoinLobby(const OnlineJoinLobbyOptions& options)
        {
            return impl->JoinLobby(options);
        }

        OnlineResult EosOnlineLobby::LeaveLobby()
        {
            return impl->LeaveLobby();
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

        const std::vector<OnlineLobbyInfo>& EosOnlineLobby::GetSearchResults() const
        {
            return impl->GetSearchResults();
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
