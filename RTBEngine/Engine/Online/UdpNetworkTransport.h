#pragma once

#include "IOnlineTransport.h"
#include "UdpSocket.h"

#include <deque>
#include <mutex>
#include <string>
#include <unordered_set>

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API UdpNetworkTransport final : public IOnlineTransport {
        public:
            // Opens a UDP socket on 0.0.0.0:port for gameplay traffic.
            bool Bind(std::uint16_t port, std::string& outError);
            // Closes socket and clears pending/received packet queues.
            void Unbind();

            bool IsAvailable() const override;
            // Wraps payload in RTBR/RTBU header and sends to the peer endpoint.
            OnlineResult SendPacket(
                const OnlineUserId& remoteUserId,
                std::uint8_t channel,
                const void* data,
                std::uint32_t size,
                OnlinePacketReliability reliability) override;
            // Dequeues one packet received since the last call (also pumps the socket).
            bool ReceivePacket(OnlinePacket& outPacket) override;
            void CloseConnections() override;
            const char* GetLastError() const override;

            // Reads all pending datagrams, handles acks/retries, fills receivedPackets.
            void PumpIncoming();

        private:
            struct PendingReliablePacket {
                OnlineUserId destination;
                std::uint8_t channel = 0;
                std::vector<std::uint8_t> payload;
                std::uint32_t sequence = 0;
                int retriesRemaining = 6;
            };

            static OnlineUserId MakePeerId(const UdpEndpoint& endpoint);
            static bool TryParsePeerId(const OnlineUserId& userId, UdpEndpoint& outEndpoint);

            UdpSocket socket;
            std::uint16_t boundPort = 0;
            std::uint32_t nextSequence = 1;
            std::deque<PendingReliablePacket> pendingReliablePackets;
            std::deque<std::string> receivedReliablePacketOrder;
            std::unordered_set<std::string> receivedReliablePacketKeys;
            std::deque<OnlinePacket> receivedPackets;
            std::mutex packetMutex;
            std::string lastError;
        };
#pragma warning(pop)

    }
}
