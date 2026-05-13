#include "EosP2PTransport.h"

#include "IOnlineIdentity.h"
#include "../Core/Logger.h"

#include <eos_common.h>
#include <eos_p2p.h>
#include <eos_p2p_types.h>
#include <eos_sdk.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

    constexpr const char* kSocketName = "RTBEngineP2P";

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

    bool IsSocketMatch(const EOS_P2P_SocketId* socketId)
    {
        return socketId && std::strcmp(socketId->SocketName, kSocketName) == 0;
    }

    void ConfigureSocket(EOS_P2P_SocketId& socketId)
    {
        socketId = {};
        socketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
        strcpy_s(socketId.SocketName, EOS_P2P_SOCKETID_SOCKETNAME_SIZE, kSocketName);
    }

}

namespace RTBEngine {
    namespace Online {

        class EosP2PTransport::Impl {
        public:
            explicit Impl(IOnlineIdentity* identity)
                : identity(identity)
            {
                ConfigureSocket(socketId);
            }

            void SetPlatformHandle(void* handle)
            {
                ResetPlatformHandle();
                platformHandle = handle;
                p2pHandle = platformHandle
                    ? EOS_Platform_GetP2PInterface(static_cast<EOS_HPlatform>(platformHandle))
                    : nullptr;

                if (!p2pHandle) {
                    return;
                }

                EOS_P2P_SetRelayControlOptions relayOptions{};
                relayOptions.ApiVersion = EOS_P2P_SETRELAYCONTROL_API_LATEST;
                relayOptions.RelayControl = EOS_ERelayControl::EOS_RC_AllowRelays;
                const EOS_EResult relayResult = EOS_P2P_SetRelayControl(p2pHandle, &relayOptions);
                if (relayResult != EOS_EResult::EOS_Success) {
                    RTB_WARN("OnlineTransport: EOS_P2P_SetRelayControl failed: " + EosResultToString(relayResult));
                }
            }

            void ResetPlatformHandle()
            {
                UnregisterNotifications();
                p2pHandle = nullptr;
                platformHandle = nullptr;
                registeredLocalUserId = {};
                lastError.clear();
            }

            bool IsAvailable() const
            {
                return p2pHandle != nullptr && identity && identity->IsLoggedIn();
            }

            OnlineResult SendPacket(
                const OnlineUserId& remoteUserId,
                std::uint8_t channel,
                const void* data,
                std::uint32_t size,
                OnlinePacketReliability reliability)
            {
                if (!p2pHandle) {
                    return Fail("EOS P2P interface is not available.", OnlineErrorCode::InvalidState);
                }

                if (!identity || !identity->IsLoggedIn()) {
                    return Fail("Cannot send EOS P2P packets without a logged-in local identity.", OnlineErrorCode::InvalidState);
                }

                if (size > EOS_P2P_MAX_PACKET_SIZE) {
                    return Fail("EOS P2P packet is larger than EOS_P2P_MAX_PACKET_SIZE.", OnlineErrorCode::InvalidConfig);
                }

                EOS_ProductUserId localProductUserId = ResolveLocalProductUserId();
                EOS_ProductUserId remoteProductUserId = ResolveRemoteProductUserId(remoteUserId);
                if (!localProductUserId || !remoteProductUserId) {
                    return Fail("EOS P2P packet needs valid local and remote Product User IDs.", OnlineErrorCode::InvalidState);
                }

                EnsureNotificationsRegistered(localProductUserId);
                AcceptConnection(localProductUserId, remoteProductUserId);

                EOS_P2P_SendPacketOptions options{};
                options.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
                options.LocalUserId = localProductUserId;
                options.RemoteUserId = remoteProductUserId;
                options.SocketId = &socketId;
                options.Channel = channel;
                options.DataLengthBytes = size;
                options.Data = data;
                options.bAllowDelayedDelivery = EOS_TRUE;
                options.Reliability = reliability == OnlinePacketReliability::Reliable
                    ? EOS_EPacketReliability::EOS_PR_ReliableOrdered
                    : EOS_EPacketReliability::EOS_PR_UnreliableUnordered;
                options.bDisableAutoAcceptConnection = EOS_FALSE;

                const EOS_EResult result = EOS_P2P_SendPacket(p2pHandle, &options);
                if (result != EOS_EResult::EOS_Success) {
                    return Fail("EOS_P2P_SendPacket failed: " + EosResultToString(result), OnlineErrorCode::BackendError);
                }

                lastError.clear();
                return OnlineResult::Success("EOS P2P packet queued.");
            }

            bool ReceivePacket(OnlinePacket& outPacket)
            {
                if (!p2pHandle || !identity || !identity->IsLoggedIn()) {
                    return false;
                }

                EOS_ProductUserId localProductUserId = ResolveLocalProductUserId();
                if (!localProductUserId) {
                    return false;
                }

                EnsureNotificationsRegistered(localProductUserId);

                EOS_P2P_GetNextReceivedPacketSizeOptions sizeOptions{};
                sizeOptions.ApiVersion = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
                sizeOptions.LocalUserId = localProductUserId;
                sizeOptions.RequestedChannel = nullptr;

                std::uint32_t packetSize = 0;
                const EOS_EResult sizeResult = EOS_P2P_GetNextReceivedPacketSize(p2pHandle, &sizeOptions, &packetSize);
                if (sizeResult == EOS_EResult::EOS_NotFound) {
                    return false;
                }

                if (sizeResult != EOS_EResult::EOS_Success || packetSize == 0 || packetSize > EOS_P2P_MAX_PACKET_SIZE) {
                    lastError = "EOS_P2P_GetNextReceivedPacketSize failed: " + EosResultToString(sizeResult);
                    return false;
                }

                std::vector<std::uint8_t> buffer(packetSize);
                EOS_ProductUserId peerId = nullptr;
                EOS_P2P_SocketId receivedSocketId{};
                std::uint8_t channel = 0;
                std::uint32_t bytesWritten = 0;

                EOS_P2P_ReceivePacketOptions receiveOptions{};
                receiveOptions.ApiVersion = EOS_P2P_RECEIVEPACKET_API_LATEST;
                receiveOptions.LocalUserId = localProductUserId;
                receiveOptions.MaxDataSizeBytes = packetSize;
                receiveOptions.RequestedChannel = nullptr;

                const EOS_EResult receiveResult = EOS_P2P_ReceivePacket(
                    p2pHandle,
                    &receiveOptions,
                    &peerId,
                    &receivedSocketId,
                    &channel,
                    buffer.data(),
                    &bytesWritten);

                if (receiveResult != EOS_EResult::EOS_Success) {
                    lastError = "EOS_P2P_ReceivePacket failed: " + EosResultToString(receiveResult);
                    return false;
                }

                if (!IsSocketMatch(&receivedSocketId)) {
                    return false;
                }

                buffer.resize(std::min<std::uint32_t>(bytesWritten, packetSize));
                outPacket.senderUserId = OnlineUserId(OnlineUserIdType::EOSProductUser, ProductUserIdToString(peerId));
                outPacket.channel = channel;
                outPacket.payload = std::move(buffer);
                lastError.clear();
                return true;
            }

            void CloseConnections()
            {
                if (!p2pHandle) {
                    return;
                }

                EOS_ProductUserId localProductUserId = ResolveLocalProductUserId();
                if (!localProductUserId) {
                    return;
                }

                EOS_P2P_CloseConnectionsOptions options{};
                options.ApiVersion = EOS_P2P_CLOSECONNECTIONS_API_LATEST;
                options.LocalUserId = localProductUserId;
                options.SocketId = &socketId;
                EOS_P2P_CloseConnections(p2pHandle, &options);
            }

            const char* GetLastError() const
            {
                return lastError.c_str();
            }

        private:
            OnlineResult Fail(const std::string& message, OnlineErrorCode code)
            {
                lastError = message;
                RTB_WARN("OnlineTransport: " + lastError);
                return OnlineResult::Failure(code, lastError);
            }

            EOS_ProductUserId ResolveLocalProductUserId() const
            {
                if (!identity) {
                    return nullptr;
                }

                const OnlineUserId& localUserId = identity->GetLocalUserId();
                return ResolveRemoteProductUserId(localUserId);
            }

            EOS_ProductUserId ResolveRemoteProductUserId(const OnlineUserId& userId) const
            {
                if (userId.GetType() != OnlineUserIdType::EOSProductUser || userId.GetValue().empty()) {
                    return nullptr;
                }

                EOS_ProductUserId productUserId = EOS_ProductUserId_FromString(userId.GetValue().c_str());
                return EOS_ProductUserId_IsValid(productUserId) == EOS_TRUE ? productUserId : nullptr;
            }

            void EnsureNotificationsRegistered(EOS_ProductUserId localProductUserId)
            {
                const std::string localUserText = ProductUserIdToString(localProductUserId);
                if (localUserText.empty()) {
                    return;
                }

                if (connectionRequestNotificationId != EOS_INVALID_NOTIFICATIONID &&
                    registeredLocalUserId == localUserText) {
                    return;
                }

                UnregisterNotifications();
                registeredLocalUserId = localUserText;

                EOS_P2P_AddNotifyPeerConnectionRequestOptions requestOptions{};
                requestOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
                requestOptions.LocalUserId = localProductUserId;
                requestOptions.SocketId = &socketId;
                connectionRequestNotificationId = EOS_P2P_AddNotifyPeerConnectionRequest(
                    p2pHandle,
                    &requestOptions,
                    this,
                    &Impl::OnIncomingConnectionRequest);

                EOS_P2P_AddNotifyPeerConnectionEstablishedOptions establishedOptions{};
                establishedOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONESTABLISHED_API_LATEST;
                establishedOptions.LocalUserId = localProductUserId;
                establishedOptions.SocketId = &socketId;
                connectionEstablishedNotificationId = EOS_P2P_AddNotifyPeerConnectionEstablished(
                    p2pHandle,
                    &establishedOptions,
                    this,
                    &Impl::OnConnectionEstablished);

                EOS_P2P_AddNotifyPeerConnectionClosedOptions closedOptions{};
                closedOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONCLOSED_API_LATEST;
                closedOptions.LocalUserId = localProductUserId;
                closedOptions.SocketId = &socketId;
                connectionClosedNotificationId = EOS_P2P_AddNotifyPeerConnectionClosed(
                    p2pHandle,
                    &closedOptions,
                    this,
                    &Impl::OnConnectionClosed);
            }

            void UnregisterNotifications()
            {
                if (!p2pHandle) {
                    connectionRequestNotificationId = EOS_INVALID_NOTIFICATIONID;
                    connectionEstablishedNotificationId = EOS_INVALID_NOTIFICATIONID;
                    connectionClosedNotificationId = EOS_INVALID_NOTIFICATIONID;
                    return;
                }

                if (connectionRequestNotificationId != EOS_INVALID_NOTIFICATIONID) {
                    EOS_P2P_RemoveNotifyPeerConnectionRequest(p2pHandle, connectionRequestNotificationId);
                    connectionRequestNotificationId = EOS_INVALID_NOTIFICATIONID;
                }

                if (connectionEstablishedNotificationId != EOS_INVALID_NOTIFICATIONID) {
                    EOS_P2P_RemoveNotifyPeerConnectionEstablished(p2pHandle, connectionEstablishedNotificationId);
                    connectionEstablishedNotificationId = EOS_INVALID_NOTIFICATIONID;
                }

                if (connectionClosedNotificationId != EOS_INVALID_NOTIFICATIONID) {
                    EOS_P2P_RemoveNotifyPeerConnectionClosed(p2pHandle, connectionClosedNotificationId);
                    connectionClosedNotificationId = EOS_INVALID_NOTIFICATIONID;
                }
            }

            void AcceptConnection(EOS_ProductUserId localProductUserId, EOS_ProductUserId remoteProductUserId)
            {
                EOS_P2P_AcceptConnectionOptions options{};
                options.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
                options.LocalUserId = localProductUserId;
                options.RemoteUserId = remoteProductUserId;
                options.SocketId = &socketId;
                EOS_P2P_AcceptConnection(p2pHandle, &options);
            }

            static void EOS_CALL OnIncomingConnectionRequest(const EOS_P2P_OnIncomingConnectionRequestInfo* data)
            {
                if (!data || !data->ClientData || !IsSocketMatch(data->SocketId)) {
                    return;
                }

                Impl* self = static_cast<Impl*>(data->ClientData);
                self->AcceptConnection(data->LocalUserId, data->RemoteUserId);
                RTB_INFO("OnlineTransport: accepted EOS P2P connection request from " +
                    ProductUserIdToString(data->RemoteUserId));
            }

            static void EOS_CALL OnConnectionEstablished(const EOS_P2P_OnPeerConnectionEstablishedInfo* data)
            {
                if (!data || !IsSocketMatch(data->SocketId)) {
                    return;
                }

                RTB_INFO("OnlineTransport: EOS P2P connection established with " +
                    ProductUserIdToString(data->RemoteUserId));
            }

            static void EOS_CALL OnConnectionClosed(const EOS_P2P_OnRemoteConnectionClosedInfo* data)
            {
                if (!data || !IsSocketMatch(data->SocketId)) {
                    return;
                }

                RTB_WARN("OnlineTransport: EOS P2P connection closed with " +
                    ProductUserIdToString(data->RemoteUserId));
            }

            IOnlineIdentity* identity = nullptr;
            void* platformHandle = nullptr;
            EOS_HP2P p2pHandle = nullptr;
            EOS_P2P_SocketId socketId{};
            EOS_NotificationId connectionRequestNotificationId = EOS_INVALID_NOTIFICATIONID;
            EOS_NotificationId connectionEstablishedNotificationId = EOS_INVALID_NOTIFICATIONID;
            EOS_NotificationId connectionClosedNotificationId = EOS_INVALID_NOTIFICATIONID;
            std::string registeredLocalUserId;
            std::string lastError;
        };

        EosP2PTransport::EosP2PTransport(IOnlineIdentity* identity)
            : impl(std::make_unique<Impl>(identity))
        {
        }

        EosP2PTransport::~EosP2PTransport() = default;

        void EosP2PTransport::SetPlatformHandle(void* handle)
        {
            impl->SetPlatformHandle(handle);
        }

        void EosP2PTransport::ResetPlatformHandle()
        {
            impl->ResetPlatformHandle();
        }

        bool EosP2PTransport::IsAvailable() const
        {
            return impl->IsAvailable();
        }

        OnlineResult EosP2PTransport::SendPacket(
            const OnlineUserId& remoteUserId,
            std::uint8_t channel,
            const void* data,
            std::uint32_t size,
            OnlinePacketReliability reliability)
        {
            return impl->SendPacket(remoteUserId, channel, data, size, reliability);
        }

        bool EosP2PTransport::ReceivePacket(OnlinePacket& outPacket)
        {
            return impl->ReceivePacket(outPacket);
        }

        void EosP2PTransport::CloseConnections()
        {
            impl->CloseConnections();
        }

        const char* EosP2PTransport::GetLastError() const
        {
            return impl->GetLastError();
        }

    }
}
