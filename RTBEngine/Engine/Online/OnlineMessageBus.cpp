#include "OnlineMessageBus.h"

#include "IOnlineIdentity.h"
#include "IOnlineLobby.h"
#include "OnlineMessageCodec.h"
#include "OnlineSystem.h"
#include "../Core/Logger.h"

#include <unordered_map>

namespace RTBEngine {
    namespace Online {

        namespace {

            std::unordered_map<std::uint16_t, OnlineMessageHandler> g_handlers;

            IOnlineLobby* GetLobby()
            {
                return OnlineSystem::GetInstance().GetLobby();
            }

            IOnlineIdentity* GetIdentity()
            {
                return OnlineSystem::GetInstance().GetIdentity();
            }

            IOnlineTransport* GetTransport()
            {
                return OnlineSystem::GetInstance().GetTransport();
            }

            bool IsLocalUser(const OnlineUserId& userId)
            {
                IOnlineIdentity* identity = GetIdentity();
                return identity && userId == identity->GetLocalUserId();
            }

            std::vector<OnlineUserId> GetRemoteLobbyMembers()
            {
                std::vector<OnlineUserId> peers;

                IOnlineLobby* lobby = GetLobby();
                if (!lobby || lobby->GetCurrentLobby().lobbyId.empty()) {
                    return peers;
                }

                const OnlineLobbyInfo& lobbyInfo = lobby->GetCurrentLobby();
                for (const OnlineUserId& member : lobbyInfo.memberUserIds) {
                    if (member.IsValid() && !IsLocalUser(member)) {
                        peers.push_back(member);
                    }
                }

                return peers;
            }

            bool SendFramedToHost(
                const std::vector<std::uint8_t>& framedBytes,
                std::uint8_t channel,
                OnlinePacketReliability reliability)
            {
                IOnlineTransport* transport = GetTransport();
                IOnlineLobby* lobby = GetLobby();
                if (!transport || !transport->IsAvailable() || !lobby || lobby->GetCurrentLobby().lobbyId.empty()) {
                    return false;
                }

                const OnlineUserId hostUserId = lobby->GetCurrentLobby().ownerUserId;
                if (!hostUserId.IsValid()) {
                    return false;
                }

                const OnlineResult result =
                    transport->SendPacket(hostUserId, channel, framedBytes, reliability);
                if (!result.success) {
                    RTB_WARN("OnlineMessageBus: failed to send packet to host. " + result.message);
                }

                return result.success;
            }

            bool SendFramedToRemoteMembers(
                const std::vector<std::uint8_t>& framedBytes,
                std::uint8_t channel,
                OnlinePacketReliability reliability)
            {
                IOnlineTransport* transport = GetTransport();
                if (!transport || !transport->IsAvailable()) {
                    return false;
                }

                bool sentAny = false;
                for (const OnlineUserId& peer : GetRemoteLobbyMembers()) {
                    const OnlineResult result =
                        transport->SendPacket(peer, channel, framedBytes, reliability);
                    if (!result.success) {
                        RTB_WARN("OnlineMessageBus: failed to send packet to " + peer.ToString() +
                            ". " + result.message);
                    }
                    sentAny = sentAny || result.success;
                }

                return sentAny;
            }

            void DispatchPacket(const OnlinePacket& packet)
            {
                if (packet.payload.size() < OnlineMessageCodec::kHeaderSize) {
                    return;
                }

                std::size_t offset = 0;
                std::uint16_t messageId = 0;
                if (!OnlineMessageCodec::ReadHeader(
                        packet.payload.data(),
                        packet.payload.size(),
                        offset,
                        messageId)) {
                    return;
                }

                const auto handlerIt = g_handlers.find(messageId);
                if (handlerIt == g_handlers.end() || !handlerIt->second) {
                    return;
                }

                OnlineMessageContext context;
                context.senderUserId = packet.senderUserId;
                context.channel = packet.channel;
                context.payload = packet.payload.data() + offset;
                context.payloadSize = packet.payload.size() - offset;
                handlerIt->second(context);
            }

        }

        void OnlineMessageBus::RegisterHandler(std::uint16_t messageId, OnlineMessageHandler handler)
        {
            if (handler) {
                g_handlers[messageId] = handler;
            }
        }

        void OnlineMessageBus::UnregisterHandler(std::uint16_t messageId)
        {
            g_handlers.erase(messageId);
        }

        void OnlineMessageBus::ClearHandlers()
        {
            g_handlers.clear();
        }

        bool OnlineMessageBus::SendToHost(
            std::uint16_t messageId,
            const std::vector<std::uint8_t>& payload,
            std::uint8_t channel,
            OnlinePacketReliability reliability)
        {
            return SendFramedToHost(OnlineMessageCodec::BuildFramedMessage(messageId, payload), channel, reliability);
        }

        bool OnlineMessageBus::BroadcastToClients(
            std::uint16_t messageId,
            const std::vector<std::uint8_t>& payload,
            std::uint8_t channel,
            OnlinePacketReliability reliability)
        {
            return SendFramedToRemoteMembers(
                OnlineMessageCodec::BuildFramedMessage(messageId, payload),
                channel,
                reliability);
        }

        void OnlineMessageBus::Pump()
        {
            IOnlineTransport* transport = GetTransport();
            if (!transport || !transport->IsAvailable()) {
                return;
            }

            OnlinePacket packet;
            constexpr int kMaxPacketsPerFrame = 64;
            for (int i = 0; i < kMaxPacketsPerFrame && transport->ReceivePacket(packet); ++i) {
                DispatchPacket(packet);
                packet = {};
            }
        }

    }
}
