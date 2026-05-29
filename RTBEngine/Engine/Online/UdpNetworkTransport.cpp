#include "UdpNetworkTransport.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <string>

namespace RTBEngine {
    namespace Online {

        namespace {

            constexpr char kReliablePrefix[] = "RTBR";     // wire magic: reliable datagram
            constexpr char kUnreliablePrefix[] = "RTBU";   // wire magic: fire-and-forget datagram
            constexpr std::size_t kPrefixSize = 4;         // all transport magics are 4 ASCII bytes
            constexpr std::size_t kChannelOffset = 4;        // byte index of logical channel id
            constexpr std::size_t kSequenceOffset = 5;     // u32 sequence starts here (after channel)
            constexpr std::size_t kPayloadSizeOffset = 9;  // u32 payload length field
            constexpr std::size_t kHeaderSize = kPayloadSizeOffset + sizeof(std::uint32_t); // 13 bytes total
            constexpr std::uint32_t kMaxPacketSize = 1200; // stay under Ethernet MTU (~1500)
            constexpr std::size_t kMaxRememberedReliablePackets = 1024; // dedup window size

            void WriteU32(std::uint8_t* buffer, std::size_t offset, std::uint32_t value)
            {
                std::memcpy(buffer + offset, &value, sizeof(value)); // little-endian on x64 Windows
            }

            bool ReadU32(const std::uint8_t* buffer, std::size_t offset, std::uint32_t& outValue)
            {
                if (offset + sizeof(std::uint32_t) > kMaxPacketSize) {
                    return false; // would read past fixed receive buffer
                }

                std::memcpy(&outValue, buffer + offset, sizeof(outValue));
                return true;
            }

            std::string MakeReliablePacketKey(const UdpEndpoint& sender, std::uint32_t sequence)
            {
                return sender.ToAddress() + "#" + std::to_string(sequence); // unique per sender+seq
            }

        }

        OnlineUserId UdpNetworkTransport::MakePeerId(const UdpEndpoint& endpoint)
        {
            return OnlineUserId(OnlineUserIdType::NetworkPeer, endpoint.ToAddress());
        }

        bool UdpNetworkTransport::TryParsePeerId(const OnlineUserId& userId, UdpEndpoint& outEndpoint)
        {
            if (userId.GetType() != OnlineUserIdType::NetworkPeer) {
                return false; // Local ids cannot be sent to over UDP
            }

            return UdpEndpoint::TryParse(userId.GetValue(), outEndpoint);
        }

        bool UdpNetworkTransport::Bind(std::uint16_t port, std::string& outError)
        {
            Unbind(); // reset socket and queues before rebinding

            if (!InitializeWinsock(outError)) {
                return false; // Winsock not available
            }

            if (!socket.Open("0.0.0.0", port, false, outError)) {
                return false; // port busy or invalid
            }

            boundPort = port; // remember for diagnostics
            lastError.clear();
            return true;
        }

        void UdpNetworkTransport::Unbind()
        {
            socket.Close(); // release kernel UDP socket
            boundPort = 0;

            std::lock_guard<std::mutex> lock(packetMutex); // exclusive access to all queues
            pendingReliablePackets.clear();      // drop unacked outbound reliable copies
            receivedReliablePacketOrder.clear(); // LRU order for dedup key eviction
            receivedReliablePacketKeys.clear();  // set of seen reliable keys
            receivedPackets.clear();             // decoded packets waiting for ReceivePacket
        }

        bool UdpNetworkTransport::IsAvailable() const
        {
            return socket.IsOpen(); // transport ready when bound socket exists
        }

        OnlineResult UdpNetworkTransport::SendPacket(
            const OnlineUserId& remoteUserId,
            std::uint8_t channel,
            const void* data,
            std::uint32_t size,
            OnlinePacketReliability reliability)
        {
            if (!socket.IsOpen()) {
                lastError = "UDP transport is not bound.";
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
            }

            UdpEndpoint destination;
            if (!TryParsePeerId(remoteUserId, destination)) {
                lastError = "Remote user id is not a valid network peer address.";
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, lastError);
            }

            if (!data || size == 0 || size > kMaxPacketSize - kHeaderSize) {
                lastError = "Packet payload is empty or too large.";
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, lastError);
            }

            std::array<std::uint8_t, kMaxPacketSize> buffer{}; // stack buffer for one wire frame
            const char* prefix = reliability == OnlinePacketReliability::Reliable ? kReliablePrefix : kUnreliablePrefix;
            std::memcpy(buffer.data(), prefix, kPrefixSize); // write 4-byte frame type tag
            buffer[kChannelOffset] = channel; // gameplay logical channel (opaque to transport)
            const std::uint32_t sequence = nextSequence++; // monotonic id for ack matching
            WriteU32(buffer.data(), kSequenceOffset, sequence);
            WriteU32(buffer.data(), kPayloadSizeOffset, size);
            std::memcpy(buffer.data() + kHeaderSize, data, size); // RTBN or other payload bytes

            std::string sendError;
            if (!socket.SendTo(buffer.data(), kHeaderSize + size, destination, sendError)) {
                lastError = sendError;
                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
            }

            if (reliability == OnlinePacketReliability::Reliable) {
                std::lock_guard<std::mutex> lock(packetMutex);
                pendingReliablePackets.push_back({
                    remoteUserId, // destination for retries
                    channel,
                    std::vector<std::uint8_t>(
                        static_cast<const std::uint8_t*>(data),
                        static_cast<const std::uint8_t*>(data) + size), // copy payload for resend
                    sequence, // must match ack RTBA sequence field
                    6         // retry budget per PumpIncoming pass
                });
            }

            lastError.clear();
            return OnlineResult::Success("UDP packet sent.");
        }

        void UdpNetworkTransport::PumpIncoming()
        {
            if (!socket.IsOpen()) {
                return; // nothing to pump without bound socket
            }

            std::array<std::uint8_t, kMaxPacketSize> buffer{}; // reuse buffer for each datagram
            while (true) {
                UdpEndpoint sender; // who sent this datagram (IP + source port)
                std::uint32_t bytesRead = 0;
                std::string receiveError;
                if (!socket.ReceiveFrom(buffer.data(), static_cast<std::uint32_t>(buffer.size()), bytesRead, sender, receiveError)) {
                    break; // queue empty (WSAEWOULDBLOCK) or fatal recv error
                }

                if (std::memcmp(buffer.data(), "RTBA", kPrefixSize) == 0 && bytesRead >= 8) {
                    // this datagram is an ack, not gameplay payload
                    std::uint32_t ackSequence = 0;
                    if (ReadU32(buffer.data(), kPrefixSize, ackSequence)) {
                        std::lock_guard<std::mutex> lock(packetMutex);
                        pendingReliablePackets.erase(
                            std::remove_if(
                                pendingReliablePackets.begin(),
                                pendingReliablePackets.end(),
                                [&](const PendingReliablePacket& packet) {
                                    return packet.sequence == ackSequence; // drop matching outbound copy
                                }),
                            pendingReliablePackets.end());
                    }
                    continue; // do not enqueue ack as OnlinePacket
                }

                if (bytesRead < kHeaderSize) {
                    continue; // truncated or foreign packet — ignore
                }

                const bool reliable = std::memcmp(buffer.data(), kReliablePrefix, kPrefixSize) == 0;
                const bool unreliable = std::memcmp(buffer.data(), kUnreliablePrefix, kPrefixSize) == 0;
                if (!reliable && !unreliable) {
                    continue; // not our transport framing (could be stray discovery text)
                }

                std::uint32_t payloadSize = 0;
                std::uint32_t sequence = 0;
                if (!ReadU32(buffer.data(), kSequenceOffset, sequence) ||
                    !ReadU32(buffer.data(), kPayloadSizeOffset, payloadSize)) {
                    continue; // corrupt header
                }

                if (payloadSize == 0 || kHeaderSize + payloadSize > bytesRead) {
                    continue; // length field lies about datagram size
                }

                bool duplicateReliablePacket = false;
                if (reliable) {
                    std::array<std::uint8_t, 16> ackBuffer{};
                    std::memcpy(ackBuffer.data(), "RTBA", kPrefixSize);
                    WriteU32(ackBuffer.data(), kPrefixSize, sequence); // echo sequence back to sender
                    std::string ackError;
                    socket.SendTo(ackBuffer.data(), 8, sender, ackError); // 4 magic + 4 sequence bytes

                    const std::string packetKey = MakeReliablePacketKey(sender, sequence);
                    std::lock_guard<std::mutex> lock(packetMutex);
                    duplicateReliablePacket = receivedReliablePacketKeys.find(packetKey) != receivedReliablePacketKeys.end();
                    if (!duplicateReliablePacket) {
                        receivedReliablePacketKeys.insert(packetKey);
                        receivedReliablePacketOrder.push_back(packetKey); // track insertion order
                        while (receivedReliablePacketOrder.size() > kMaxRememberedReliablePackets) {
                            receivedReliablePacketKeys.erase(receivedReliablePacketOrder.front()); // evict oldest
                            receivedReliablePacketOrder.pop_front();
                        }
                    }
                }

                if (duplicateReliablePacket) {
                    continue; // already delivered this reliable seq to gameplay
                }

                OnlinePacket packet;
                packet.senderUserId = MakePeerId(sender); // gameplay sees NetworkPeer:id
                packet.channel = buffer[kChannelOffset];
                packet.payload.assign(
                    buffer.begin() + kHeaderSize,
                    buffer.begin() + kHeaderSize + payloadSize); // strip transport header

                std::lock_guard<std::mutex> lock(packetMutex);
                receivedPackets.push_back(std::move(packet)); // FIFO for ReceivePacket
            }

            std::lock_guard<std::mutex> lock(packetMutex);
            for (auto it = pendingReliablePackets.begin(); it != pendingReliablePackets.end();) {
                if (it->retriesRemaining <= 0) {
                    it = pendingReliablePackets.erase(it); // give up on this reliable send
                    continue;
                }

                UdpEndpoint destination;
                if (!TryParsePeerId(it->destination, destination)) {
                    it = pendingReliablePackets.erase(it); // destination id became invalid
                    continue;
                }

                std::array<std::uint8_t, kMaxPacketSize> resendBuffer{};
                std::memcpy(resendBuffer.data(), kReliablePrefix, kPrefixSize);
                resendBuffer[kChannelOffset] = it->channel;
                WriteU32(resendBuffer.data(), kSequenceOffset, it->sequence); // same seq as original send
                WriteU32(resendBuffer.data(), kPayloadSizeOffset, static_cast<std::uint32_t>(it->payload.size()));
                std::memcpy(
                    resendBuffer.data() + kHeaderSize,
                    it->payload.data(),
                    it->payload.size());

                std::string sendError;
                socket.SendTo(
                    resendBuffer.data(),
                    kHeaderSize + static_cast<std::uint32_t>(it->payload.size()),
                    destination,
                    sendError); // ignore sendError — retry again next Pump

                --it->retriesRemaining;
                ++it;
            }
        }

        bool UdpNetworkTransport::ReceivePacket(OnlinePacket& outPacket)
        {
            PumpIncoming(); // always refresh queues before dequeue

            std::lock_guard<std::mutex> lock(packetMutex);
            if (receivedPackets.empty()) {
                return false; // no decoded packet ready — caller tries again next frame
            }

            outPacket = std::move(receivedPackets.front()); // transfer ownership without copy
            receivedPackets.pop_front();
            return true;
        }

        void UdpNetworkTransport::CloseConnections()
        {
            Unbind(); // alias for full socket + queue teardown
            lastError.clear();
        }

        const char* UdpNetworkTransport::GetLastError() const
        {
            return lastError.c_str(); // pointer valid until next mutating call on this transport
        }

    }
}
