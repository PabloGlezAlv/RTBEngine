#include "UdpNetworkTransport.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <string>

namespace RTBEngine {
    namespace Online {

        namespace {

            constexpr char kReliablePrefix[] = "RTBR";
            constexpr char kUnreliablePrefix[] = "RTBU";
            constexpr std::size_t kPrefixSize = 4;
            constexpr std::size_t kChannelOffset = 4;
            constexpr std::size_t kSequenceOffset = 5;
            constexpr std::size_t kPayloadSizeOffset = 9;
            constexpr std::size_t kHeaderSize = kPayloadSizeOffset + sizeof(std::uint32_t);
            constexpr std::uint32_t kMaxPacketSize = 1200;
            constexpr std::size_t kMaxRememberedReliablePackets = 1024;

            void WriteU32(std::uint8_t* buffer, std::size_t offset, std::uint32_t value)
            {
                std::memcpy(buffer + offset, &value, sizeof(value));
            }

            bool ReadU32(const std::uint8_t* buffer, std::size_t offset, std::uint32_t& outValue)
            {
                if (offset + sizeof(std::uint32_t) > kMaxPacketSize) {
                    return false;
                }

                std::memcpy(&outValue, buffer + offset, sizeof(outValue));
                return true;
            }

            std::string MakeReliablePacketKey(const UdpEndpoint& sender, std::uint32_t sequence)
            {
                return sender.ToAddress() + "#" + std::to_string(sequence);
            }

        }

        OnlineUserId UdpNetworkTransport::MakePeerId(const UdpEndpoint& endpoint)
        {
            return OnlineUserId(OnlineUserIdType::NetworkPeer, endpoint.ToAddress());
        }

        bool UdpNetworkTransport::TryParsePeerId(const OnlineUserId& userId, UdpEndpoint& outEndpoint)
        {
            if (userId.GetType() != OnlineUserIdType::NetworkPeer) {
                return false;
            }

            return UdpEndpoint::TryParse(userId.GetValue(), outEndpoint);
        }

        bool UdpNetworkTransport::Bind(std::uint16_t port, std::string& outError)
        {
            Unbind();

            if (!InitializeWinsock(outError)) {
                return false;
            }

            if (!socket.Open("0.0.0.0", port, false, outError)) {
                return false;
            }

            boundPort = port;
            lastError.clear();
            return true;
        }

        void UdpNetworkTransport::Unbind()
        {
            socket.Close();
            boundPort = 0;

            std::lock_guard<std::mutex> lock(packetMutex);
            pendingReliablePackets.clear();
            receivedReliablePacketOrder.clear();
            receivedReliablePacketKeys.clear();
            receivedPackets.clear();
        }

        bool UdpNetworkTransport::IsAvailable() const
        {
            return socket.IsOpen();
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

            std::array<std::uint8_t, kMaxPacketSize> buffer{};
            const char* prefix = reliability == OnlinePacketReliability::Reliable ? kReliablePrefix : kUnreliablePrefix;
            std::memcpy(buffer.data(), prefix, kPrefixSize);
            buffer[kChannelOffset] = channel;
            const std::uint32_t sequence = nextSequence++;
            WriteU32(buffer.data(), kSequenceOffset, sequence);
            WriteU32(buffer.data(), kPayloadSizeOffset, size);
            std::memcpy(buffer.data() + kHeaderSize, data, size);

            std::string sendError;
            if (!socket.SendTo(buffer.data(), kHeaderSize + size, destination, sendError)) {
                lastError = sendError;
                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
            }

            if (reliability == OnlinePacketReliability::Reliable) {
                std::lock_guard<std::mutex> lock(packetMutex);
                pendingReliablePackets.push_back({
                    remoteUserId,
                    channel,
                    std::vector<std::uint8_t>(
                        static_cast<const std::uint8_t*>(data),
                        static_cast<const std::uint8_t*>(data) + size),
                    sequence,
                    6
                });
            }

            lastError.clear();
            return OnlineResult::Success("UDP packet sent.");
        }

        void UdpNetworkTransport::PumpIncoming()
        {
            if (!socket.IsOpen()) {
                return;
            }

            std::array<std::uint8_t, kMaxPacketSize> buffer{};
            while (true) {
                UdpEndpoint sender;
                std::uint32_t bytesRead = 0;
                std::string receiveError;
                if (!socket.ReceiveFrom(buffer.data(), static_cast<std::uint32_t>(buffer.size()), bytesRead, sender, receiveError)) {
                    break;
                }

                if (std::memcmp(buffer.data(), "RTBA", kPrefixSize) == 0 && bytesRead >= 8) {
                    std::uint32_t ackSequence = 0;
                    if (ReadU32(buffer.data(), kPrefixSize, ackSequence)) {
                        std::lock_guard<std::mutex> lock(packetMutex);
                        pendingReliablePackets.erase(
                            std::remove_if(
                                pendingReliablePackets.begin(),
                                pendingReliablePackets.end(),
                                [&](const PendingReliablePacket& packet) {
                                    return packet.sequence == ackSequence;
                                }),
                            pendingReliablePackets.end());
                    }
                    continue;
                }

                if (bytesRead < kHeaderSize) {
                    continue;
                }

                const bool reliable = std::memcmp(buffer.data(), kReliablePrefix, kPrefixSize) == 0;
                const bool unreliable = std::memcmp(buffer.data(), kUnreliablePrefix, kPrefixSize) == 0;
                if (!reliable && !unreliable) {
                    continue;
                }

                std::uint32_t payloadSize = 0;
                std::uint32_t sequence = 0;
                if (!ReadU32(buffer.data(), kSequenceOffset, sequence) ||
                    !ReadU32(buffer.data(), kPayloadSizeOffset, payloadSize)) {
                    continue;
                }

                if (payloadSize == 0 || kHeaderSize + payloadSize > bytesRead) {
                    continue;
                }

                bool duplicateReliablePacket = false;
                if (reliable) {
                    std::array<std::uint8_t, 16> ackBuffer{};
                    std::memcpy(ackBuffer.data(), "RTBA", kPrefixSize);
                    WriteU32(ackBuffer.data(), kPrefixSize, sequence);
                    std::string ackError;
                    socket.SendTo(ackBuffer.data(), 8, sender, ackError);

                    const std::string packetKey = MakeReliablePacketKey(sender, sequence);
                    std::lock_guard<std::mutex> lock(packetMutex);
                    duplicateReliablePacket = receivedReliablePacketKeys.find(packetKey) != receivedReliablePacketKeys.end();
                    if (!duplicateReliablePacket) {
                        receivedReliablePacketKeys.insert(packetKey);
                        receivedReliablePacketOrder.push_back(packetKey);
                        while (receivedReliablePacketOrder.size() > kMaxRememberedReliablePackets) {
                            receivedReliablePacketKeys.erase(receivedReliablePacketOrder.front());
                            receivedReliablePacketOrder.pop_front();
                        }
                    }
                }

                if (duplicateReliablePacket) {
                    continue;
                }

                OnlinePacket packet;
                packet.senderUserId = MakePeerId(sender);
                packet.channel = buffer[kChannelOffset];
                packet.payload.assign(
                    buffer.begin() + kHeaderSize,
                    buffer.begin() + kHeaderSize + payloadSize);

                std::lock_guard<std::mutex> lock(packetMutex);
                receivedPackets.push_back(std::move(packet));
            }

            std::lock_guard<std::mutex> lock(packetMutex);
            for (auto it = pendingReliablePackets.begin(); it != pendingReliablePackets.end();) {
                if (it->retriesRemaining <= 0) {
                    it = pendingReliablePackets.erase(it);
                    continue;
                }

                UdpEndpoint destination;
                if (!TryParsePeerId(it->destination, destination)) {
                    it = pendingReliablePackets.erase(it);
                    continue;
                }

                std::array<std::uint8_t, kMaxPacketSize> resendBuffer{};
                std::memcpy(resendBuffer.data(), kReliablePrefix, kPrefixSize);
                resendBuffer[kChannelOffset] = it->channel;
                WriteU32(resendBuffer.data(), kSequenceOffset, it->sequence);
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
                    sendError);

                --it->retriesRemaining;
                ++it;
            }
        }

        bool UdpNetworkTransport::ReceivePacket(OnlinePacket& outPacket)
        {
            PumpIncoming();

            std::lock_guard<std::mutex> lock(packetMutex);
            if (receivedPackets.empty()) {
                return false;
            }

            outPacket = std::move(receivedPackets.front());
            receivedPackets.pop_front();
            return true;
        }

        void UdpNetworkTransport::CloseConnections()
        {
            Unbind();
            lastError.clear();
        }

        const char* UdpNetworkTransport::GetLastError() const
        {
            return lastError.c_str();
        }

    }
}
