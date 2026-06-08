#pragma once

#include "IOnlineTransport.h"
#include "RelayOnlineLobby.h"
#include "RelayWire.h"
#include "UdpSocket.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API RelayNetworkTransport final : public IOnlineTransport {
        public:
            bool Connect(const RelaySessionInfo& session, std::string& outError);
            void Disconnect();
            bool IsConnected() const { return connected; }

            void PumpIncoming();
            void TickKeepalive(float deltaTime);

            bool IsAvailable() const override;
            OnlineResult SendPacket(
                const OnlineUserId& remoteUserId,
                std::uint8_t channel,
                const void* data,
                std::uint32_t size,
                OnlinePacketReliability reliability) override;
            bool ReceivePacket(OnlinePacket& outPacket) override;
            void CloseConnections() override;
            const char* GetLastError() const override;

        private:
            struct PendingReliablePacket {
                OnlineUserId destination;
                std::uint8_t channel = 0;
                std::vector<std::uint8_t> payload;
                std::uint32_t sequence = 0;
                int retriesRemaining = 6;
            };

            static OnlineUserId MakeRelayPeerId(const std::uint8_t* memberIdBytes);
            static bool TryParseRelayPeerId(const OnlineUserId& userId, std::uint8_t* outMemberIdBytes, std::size_t capacity);

            bool SendConnectPacket();
            bool SendRelayGamePacket(
                const std::uint8_t* targetMemberIdBytes,
                const void* innerPayload,
                std::size_t innerPayloadSize);
            void ProcessInnerGameplayDatagram(
                const std::uint8_t* senderMemberIdBytes,
                const std::uint8_t* innerData,
                std::size_t innerSize);

            UdpSocket socket;
            UdpEndpoint relayEndpoint;
            std::array<std::uint8_t, kRelayIdByteLength> sessionTokenBytes{};
            std::array<std::uint8_t, kRelayIdByteLength> localMemberIdBytes{};
            bool connected = false;
            std::uint32_t nextSequence = 1;
            std::chrono::steady_clock::time_point lastConnectSendTime{};
            float connectKeepaliveTimer = 0.0f;
            std::deque<PendingReliablePacket> pendingReliablePackets;
            std::deque<std::string> receivedReliablePacketKeysOrder;
            std::unordered_set<std::string> receivedReliablePacketKeys;
            std::deque<OnlinePacket> receivedPackets;
            std::mutex packetMutex;
            std::string lastError;
        };
#pragma warning(pop)

    }
}
