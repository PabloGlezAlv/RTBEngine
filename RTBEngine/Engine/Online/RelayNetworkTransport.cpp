#include "RelayNetworkTransport.h"

#include "RelayWire.h"
#include "UdpSocket.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>

namespace RTBEngine {
    namespace Online {

        namespace {

            constexpr char kReliablePrefix[] = "RTBR";
            constexpr char kUnreliablePrefix[] = "RTBU";
            constexpr std::size_t kPrefixSize = 4;
            constexpr std::size_t kChannelOffset = 4;
            constexpr std::size_t kSequenceOffset = 5;
            constexpr std::size_t kPayloadSizeOffset = 9;
            constexpr std::size_t kInnerHeaderSize = kPayloadSizeOffset + sizeof(std::uint32_t);
            constexpr std::size_t kMaxInnerPacketSize = kRelayMaxDatagramSize - kRelayGameClientHeaderSize;
            constexpr std::size_t kMaxRememberedReliablePackets = 1024;
            constexpr float kConnectKeepaliveSeconds = 5.0f;

            void WriteU32(std::uint8_t* buffer, std::size_t offset, std::uint32_t value)
            {
                std::memcpy(buffer + offset, &value, sizeof(value));
            }

            bool ReadU32(const std::uint8_t* buffer, std::size_t offset, std::uint32_t& outValue)
            {
                if (offset + sizeof(std::uint32_t) > kRelayMaxDatagramSize) {
                    return false;
                }

                std::memcpy(&outValue, buffer + offset, sizeof(outValue));
                return true;
            }

            std::string MakeReliablePacketKey(const std::string& senderMemberHex, std::uint32_t sequence)
            {
                return senderMemberHex + "#" + std::to_string(sequence);
            }

        }

        OnlineUserId RelayNetworkTransport::MakeRelayPeerId(const std::uint8_t* memberIdBytes)
        {
            std::string memberHex;
            RelayIdBytesToHex(memberIdBytes, kRelayIdByteLength, memberHex);
            return OnlineUserId(OnlineUserIdType::RelayPeer, memberHex);
        }

        bool RelayNetworkTransport::TryParseRelayPeerId(
            const OnlineUserId& userId,
            std::uint8_t* outMemberIdBytes,
            std::size_t capacity)
        {
            if (userId.GetType() != OnlineUserIdType::RelayPeer) {
                return false;
            }

            return RelayTryHexToIdBytes(userId.GetValue(), outMemberIdBytes, capacity);
        }

        bool RelayNetworkTransport::Connect(const RelaySessionInfo& session, std::string& outError)
        {
            if (session.sessionToken.empty() || session.localMemberId.empty() ||
                session.relayHost.empty() || session.relayPort == 0) {
                outError = "Relay session info is incomplete.";
                return false;
            }

            std::array<std::uint8_t, kRelayIdByteLength> tokenBytes{};
            std::array<std::uint8_t, kRelayIdByteLength> memberBytes{};
            if (!RelayTryHexToIdBytes(session.sessionToken, tokenBytes.data(), tokenBytes.size()) ||
                !RelayTryHexToIdBytes(session.localMemberId, memberBytes.data(), memberBytes.size())) {
                outError = "Relay session token or member id is not valid hex.";
                return false;
            }

            if (!socket.IsOpen()) {
                if (!InitializeWinsock(outError)) {
                    return false;
                }

                if (!socket.Open("0.0.0.0", 0, false, outError)) {
                    return false;
                }
            }

            std::string resolvedRelayHost;
            std::string resolveError;
            if (!ResolveHostAddress(session.relayHost, resolvedRelayHost, resolveError)) {
                outError = resolveError.empty()
                    ? "Could not resolve relay host: " + session.relayHost
                    : resolveError;
                return false;
            }

            relayEndpoint.host = resolvedRelayHost;
            relayEndpoint.port = session.relayPort;
            std::memcpy(sessionTokenBytes.data(), tokenBytes.data(), kRelayIdByteLength);
            std::memcpy(localMemberIdBytes.data(), memberBytes.data(), kRelayIdByteLength);
            connected = true;
            connectKeepaliveTimer = 0.0f;
            lastConnectSendTime = std::chrono::steady_clock::now();

            if (!SendConnectPacket()) {
                outError = lastError.empty() ? "Failed to send RTBC connect packet." : lastError;
                return false;
            }

            lastError.clear();
            return true;
        }

        void RelayNetworkTransport::Disconnect()
        {
            socket.Close();
            connected = false;
            connectKeepaliveTimer = 0.0f;
            nextSequence = 1;

            std::lock_guard<std::mutex> lock(packetMutex);
            pendingReliablePackets.clear();
            receivedReliablePacketKeysOrder.clear();
            receivedReliablePacketKeys.clear();
            receivedPackets.clear();
            lastError.clear();
        }

        bool RelayNetworkTransport::SendConnectPacket()
        {
            if (!connected || !socket.IsOpen()) {
                return false;
            }

            std::array<std::uint8_t, kRelayConnectPacketSize> packet{};
            const std::size_t packetSize = RelayWriteConnectPacket(
                packet.data(),
                packet.size(),
                sessionTokenBytes.data(),
                localMemberIdBytes.data());
            if (packetSize == 0) {
                lastError = "Failed to build RTBC packet.";
                return false;
            }

            std::string sendError;
            if (!socket.SendTo(packet.data(), static_cast<std::uint32_t>(packetSize), relayEndpoint, sendError)) {
                lastError = sendError;
                return false;
            }

            lastConnectSendTime = std::chrono::steady_clock::now();
            return true;
        }

        bool RelayNetworkTransport::SendRelayGamePacket(
            const std::uint8_t* targetMemberIdBytes,
            const void* innerPayload,
            std::size_t innerPayloadSize)
        {
            std::array<std::uint8_t, kRelayMaxDatagramSize> buffer{};
            const std::size_t packetSize = RelayWriteGameClientPacket(
                buffer.data(),
                buffer.size(),
                sessionTokenBytes.data(),
                targetMemberIdBytes,
                innerPayload,
                innerPayloadSize);
            if (packetSize == 0) {
                lastError = "Failed to build RTBG packet.";
                return false;
            }

            std::string sendError;
            if (!socket.SendTo(buffer.data(), static_cast<std::uint32_t>(packetSize), relayEndpoint, sendError)) {
                lastError = sendError;
                return false;
            }

            return true;
        }

        void RelayNetworkTransport::TickKeepalive(float deltaTime)
        {
            if (!connected) {
                return;
            }

            connectKeepaliveTimer += deltaTime;
            if (connectKeepaliveTimer >= kConnectKeepaliveSeconds) {
                connectKeepaliveTimer = 0.0f;
                SendConnectPacket();
            }
        }

        bool RelayNetworkTransport::IsAvailable() const
        {
            return connected && socket.IsOpen();
        }

        OnlineResult RelayNetworkTransport::SendPacket(
            const OnlineUserId& remoteUserId,
            std::uint8_t channel,
            const void* data,
            std::uint32_t size,
            OnlinePacketReliability reliability)
        {
            if (!IsAvailable()) {
                lastError = "Relay transport is not connected.";
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
            }

            std::array<std::uint8_t, kRelayIdByteLength> targetMemberIdBytes{};
            if (!TryParseRelayPeerId(remoteUserId, targetMemberIdBytes.data(), targetMemberIdBytes.size())) {
                lastError = "Remote user id is not a valid relay peer member id.";
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, lastError);
            }

            if (!data || size == 0 || size > kMaxInnerPacketSize - kInnerHeaderSize) {
                lastError = "Packet payload is empty or too large for relay transport.";
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, lastError);
            }

            std::array<std::uint8_t, kMaxInnerPacketSize> innerBuffer{};
            const char* prefix = reliability == OnlinePacketReliability::Reliable ? kReliablePrefix : kUnreliablePrefix;
            std::memcpy(innerBuffer.data(), prefix, kPrefixSize);
            innerBuffer[kChannelOffset] = channel;
            const std::uint32_t sequence = nextSequence++;
            WriteU32(innerBuffer.data(), kSequenceOffset, sequence);
            WriteU32(innerBuffer.data(), kPayloadSizeOffset, size);
            std::memcpy(innerBuffer.data() + kInnerHeaderSize, data, size);

            const std::size_t innerSize = kInnerHeaderSize + size;
            if (!SendRelayGamePacket(targetMemberIdBytes.data(), innerBuffer.data(), innerSize)) {
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
            return OnlineResult::Success("Relay packet sent.");
        }

        void RelayNetworkTransport::ProcessInnerGameplayDatagram(
            const std::uint8_t* senderMemberIdBytes,
            const std::uint8_t* innerData,
            std::size_t innerSize)
        {
            if (!senderMemberIdBytes || !innerData || innerSize < kInnerHeaderSize) {
                return;
            }

            std::string senderMemberHex;
            RelayIdBytesToHex(senderMemberIdBytes, kRelayIdByteLength, senderMemberHex);

            if (std::memcmp(innerData, "RTBA", kPrefixSize) == 0 && innerSize >= 8) {
                std::uint32_t ackSequence = 0;
                if (ReadU32(innerData, kPrefixSize, ackSequence)) {
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
                return;
            }

            const bool reliable = std::memcmp(innerData, kReliablePrefix, kPrefixSize) == 0;
            const bool unreliable = std::memcmp(innerData, kUnreliablePrefix, kPrefixSize) == 0;
            if (!reliable && !unreliable) {
                return;
            }

            std::uint32_t payloadSize = 0;
            std::uint32_t sequence = 0;
            if (!ReadU32(innerData, kSequenceOffset, sequence) ||
                !ReadU32(innerData, kPayloadSizeOffset, payloadSize)) {
                return;
            }

            if (payloadSize == 0 || kInnerHeaderSize + payloadSize > innerSize) {
                return;
            }

            bool duplicateReliablePacket = false;
            if (reliable) {
                std::array<std::uint8_t, 16> ackBuffer{};
                std::memcpy(ackBuffer.data(), "RTBA", kPrefixSize);
                WriteU32(ackBuffer.data(), kPrefixSize, sequence);
                SendRelayGamePacket(senderMemberIdBytes, ackBuffer.data(), 8);

                const std::string packetKey = MakeReliablePacketKey(senderMemberHex, sequence);
                std::lock_guard<std::mutex> lock(packetMutex);
                duplicateReliablePacket = receivedReliablePacketKeys.find(packetKey) != receivedReliablePacketKeys.end();
                if (!duplicateReliablePacket) {
                    receivedReliablePacketKeys.insert(packetKey);
                    receivedReliablePacketKeysOrder.push_back(packetKey);
                    while (receivedReliablePacketKeysOrder.size() > kMaxRememberedReliablePackets) {
                        receivedReliablePacketKeys.erase(receivedReliablePacketKeysOrder.front());
                        receivedReliablePacketKeysOrder.pop_front();
                    }
                }
            }

            if (duplicateReliablePacket) {
                return;
            }

            OnlinePacket packet;
            packet.senderUserId = MakeRelayPeerId(senderMemberIdBytes);
            packet.channel = innerData[kChannelOffset];
            packet.payload.assign(
                innerData + kInnerHeaderSize,
                innerData + kInnerHeaderSize + payloadSize);

            std::lock_guard<std::mutex> lock(packetMutex);
            receivedPackets.push_back(std::move(packet));
        }

        void RelayNetworkTransport::PumpIncoming()
        {
            if (!socket.IsOpen()) {
                return;
            }

            std::array<std::uint8_t, kRelayMaxDatagramSize> buffer{};
            while (true) {
                UdpEndpoint sender;
                std::uint32_t bytesRead = 0;
                std::string receiveError;
                if (!socket.ReceiveFrom(buffer.data(), static_cast<std::uint32_t>(buffer.size()), bytesRead, sender, receiveError)) {
                    break;
                }

                if (bytesRead < kRelayGameServerHeaderSize) {
                    continue;
                }

                if (std::memcmp(buffer.data(), "RTBG", kPrefixSize) != 0) {
                    continue;
                }

                const std::uint8_t* senderMemberIdBytes = buffer.data() + kPrefixSize;
                const std::uint8_t* innerData = buffer.data() + kRelayGameServerHeaderSize;
                const std::size_t innerSize = bytesRead - kRelayGameServerHeaderSize;
                ProcessInnerGameplayDatagram(senderMemberIdBytes, innerData, innerSize);
            }

            std::lock_guard<std::mutex> lock(packetMutex);
            for (auto it = pendingReliablePackets.begin(); it != pendingReliablePackets.end();) {
                if (it->retriesRemaining <= 0) {
                    it = pendingReliablePackets.erase(it);
                    continue;
                }

                std::array<std::uint8_t, kRelayIdByteLength> targetMemberIdBytes{};
                if (!TryParseRelayPeerId(it->destination, targetMemberIdBytes.data(), targetMemberIdBytes.size())) {
                    it = pendingReliablePackets.erase(it);
                    continue;
                }

                std::array<std::uint8_t, kMaxInnerPacketSize> innerBuffer{};
                std::memcpy(innerBuffer.data(), kReliablePrefix, kPrefixSize);
                innerBuffer[kChannelOffset] = it->channel;
                WriteU32(innerBuffer.data(), kSequenceOffset, it->sequence);
                WriteU32(innerBuffer.data(), kPayloadSizeOffset, static_cast<std::uint32_t>(it->payload.size()));
                std::memcpy(
                    innerBuffer.data() + kInnerHeaderSize,
                    it->payload.data(),
                    it->payload.size());

                const std::size_t innerSize = kInnerHeaderSize + it->payload.size();
                SendRelayGamePacket(targetMemberIdBytes.data(), innerBuffer.data(), innerSize);

                --it->retriesRemaining;
                ++it;
            }
        }

        bool RelayNetworkTransport::ReceivePacket(OnlinePacket& outPacket)
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

        void RelayNetworkTransport::CloseConnections()
        {
            Disconnect();
        }

        const char* RelayNetworkTransport::GetLastError() const
        {
            return lastError.c_str();
        }

    }
}
