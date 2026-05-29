#include "OnlineGameplayNet.h"

#include "IOnlineIdentity.h"
#include "IOnlineLobby.h"
#include "IOnlineTransport.h"
#include "OnlineSystem.h"
#include "../Core/Logger.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace RTBEngine {
    namespace Online {

        namespace {

            constexpr std::uint32_t kMagic = 0x4E425452;
            constexpr std::uint16_t kProtocolVersion = 2;
            constexpr std::uint8_t kControlChannel = 0;
            constexpr std::uint8_t kSnapshotChannel = 1;
            constexpr std::uint8_t kInputChannel = 2;

            enum class MessageType : std::uint8_t {
                StartMatch = 1,
                TransformSnapshot = 2,
                PlayerInput = 3
            };

            struct PacketCursor {
                const std::vector<std::uint8_t>& bytes;
                std::size_t offset = 0;
            };

            std::deque<std::string> pendingStartMatches;
            std::unordered_map<std::string, OnlineGameplayNet::TransformSnapshot> latestTransforms;
            std::unordered_map<std::string, OnlineGameplayNet::PlayerInputSnapshot> latestInputs;

            template <typename T>
            void AppendValue(std::vector<std::uint8_t>& outBytes, const T& value)
            {
                const auto* raw = reinterpret_cast<const std::uint8_t*>(&value);
                outBytes.insert(outBytes.end(), raw, raw + sizeof(T));
            }

            void AppendString(std::vector<std::uint8_t>& outBytes, const std::string& value)
            {
                const std::uint16_t length = static_cast<std::uint16_t>(std::min<std::size_t>(value.size(), 1024));
                AppendValue(outBytes, length);
                outBytes.insert(outBytes.end(), value.begin(), value.begin() + length);
            }

            template <typename T>
            bool ReadValue(PacketCursor& cursor, T& outValue)
            {
                if (cursor.offset + sizeof(T) > cursor.bytes.size()) {
                    return false;
                }

                std::memcpy(&outValue, cursor.bytes.data() + cursor.offset, sizeof(T));
                cursor.offset += sizeof(T);
                return true;
            }

            bool ReadString(PacketCursor& cursor, std::string& outValue)
            {
                std::uint16_t length = 0;
                if (!ReadValue(cursor, length) || cursor.offset + length > cursor.bytes.size()) {
                    return false;
                }

                outValue.assign(
                    reinterpret_cast<const char*>(cursor.bytes.data() + cursor.offset),
                    static_cast<std::size_t>(length));
                cursor.offset += length;
                return true;
            }

            void AppendHeader(std::vector<std::uint8_t>& outBytes, MessageType messageType)
            {
                AppendValue(outBytes, kMagic);
                AppendValue(outBytes, kProtocolVersion);
                AppendValue(outBytes, static_cast<std::uint8_t>(messageType));
                const std::uint8_t reserved = 0;
                AppendValue(outBytes, reserved);
            }

            bool ReadHeader(PacketCursor& cursor, MessageType& outMessageType)
            {
                std::uint32_t magic = 0;
                std::uint16_t version = 0;
                std::uint8_t messageType = 0;
                std::uint8_t reserved = 0;

                if (!ReadValue(cursor, magic) ||
                    !ReadValue(cursor, version) ||
                    !ReadValue(cursor, messageType) ||
                    !ReadValue(cursor, reserved)) {
                    return false;
                }

                if (magic != kMagic || version != kProtocolVersion) {
                    return false;
                }

                outMessageType = static_cast<MessageType>(messageType);
                return true;
            }

            std::vector<std::uint8_t> BuildStartMatchPacket(const std::string& scenePath)
            {
                std::vector<std::uint8_t> bytes;
                AppendHeader(bytes, MessageType::StartMatch);
                AppendString(bytes, scenePath);
                return bytes;
            }

            std::vector<std::uint8_t> BuildTransformPacket(const OnlineGameplayNet::TransformSnapshot& snapshot)
            {
                std::vector<std::uint8_t> bytes;
                AppendHeader(bytes, MessageType::TransformSnapshot);
                AppendString(bytes, snapshot.objectKey);
                AppendValue(bytes, snapshot.position.x);
                AppendValue(bytes, snapshot.position.y);
                AppendValue(bytes, snapshot.position.z);
                AppendValue(bytes, snapshot.rotation.x);
                AppendValue(bytes, snapshot.rotation.y);
                AppendValue(bytes, snapshot.rotation.z);
                AppendValue(bytes, snapshot.rotation.w);
                return bytes;
            }

            std::vector<std::uint8_t> BuildPlayerInputPacket(const OnlineGameplayNet::PlayerInputSnapshot& snapshot)
            {
                std::vector<std::uint8_t> bytes;
                AppendHeader(bytes, MessageType::PlayerInput);
                AppendValue(bytes, snapshot.sequenceNumber);
                AppendValue(bytes, snapshot.moveX);
                AppendValue(bytes, snapshot.moveZ);
                const std::uint8_t sprintFlag = snapshot.sprint ? 1 : 0;
                AppendValue(bytes, sprintFlag);
                return bytes;
            }

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

            bool SendToHost(
                const std::vector<std::uint8_t>& bytes,
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
                    transport->SendPacket(hostUserId, channel, bytes, reliability);
                if (!result.success) {
                    RTB_WARN("OnlineGameplayNet: failed to send packet to host. " + result.message);
                }

                return result.success;
            }

            bool SendToRemoteLobbyMembers(
                const std::vector<std::uint8_t>& bytes,
                std::uint8_t channel,
                OnlinePacketReliability reliability)
            {
                IOnlineTransport* transport = GetTransport();
                if (!transport || !transport->IsAvailable()) {
                    RTB_WARN("OnlineGameplayNet: online transport is not available for outgoing packet.");
                    return false;
                }

                bool sentAny = false;
                for (const OnlineUserId& peer : GetRemoteLobbyMembers()) {
                    const OnlineResult result =
                        transport->SendPacket(peer, channel, bytes, reliability);
                    if (!result.success) {
                        RTB_WARN("OnlineGameplayNet: failed to send packet to " + peer.ToString() +
                            ". " + result.message);
                    }
                    sentAny = sentAny || result.success;
                }

                if (!sentAny) {
                    RTB_WARN("OnlineGameplayNet: no remote lobby members accepted the outgoing packet.");
                }

                return sentAny;
            }

            void HandlePacket(const OnlinePacket& packet)
            {
                PacketCursor cursor{ packet.payload };
                MessageType messageType = MessageType::StartMatch;
                if (!ReadHeader(cursor, messageType)) {
                    return;
                }

                if (messageType == MessageType::StartMatch) {
                    std::string scenePath;
                    if (ReadString(cursor, scenePath) && !scenePath.empty()) {
                        pendingStartMatches.push_back(scenePath);
                        RTB_INFO("OnlineGameplayNet: received StartMatch for scene " + scenePath);
                    }
                    return;
                }

                if (messageType == MessageType::TransformSnapshot) {
                    OnlineGameplayNet::TransformSnapshot snapshot;
                    snapshot.senderUserId = packet.senderUserId;
                    if (!ReadString(cursor, snapshot.objectKey) ||
                        !ReadValue(cursor, snapshot.position.x) ||
                        !ReadValue(cursor, snapshot.position.y) ||
                        !ReadValue(cursor, snapshot.position.z) ||
                        !ReadValue(cursor, snapshot.rotation.x) ||
                        !ReadValue(cursor, snapshot.rotation.y) ||
                        !ReadValue(cursor, snapshot.rotation.z) ||
                        !ReadValue(cursor, snapshot.rotation.w) ||
                        snapshot.objectKey.empty()) {
                        return;
                    }

                    latestTransforms[snapshot.objectKey] = snapshot;
                    return;
                }

                if (messageType == MessageType::PlayerInput) {
                    OnlineGameplayNet::PlayerInputSnapshot snapshot;
                    snapshot.senderUserId = packet.senderUserId;
                    std::uint8_t sprintFlag = 0;
                    if (!ReadValue(cursor, snapshot.sequenceNumber) ||
                        !ReadValue(cursor, snapshot.moveX) ||
                        !ReadValue(cursor, snapshot.moveZ) ||
                        !ReadValue(cursor, sprintFlag)) {
                        return;
                    }

                    snapshot.sprint = sprintFlag != 0;
                    latestInputs[snapshot.senderUserId.ToString()] = snapshot;
                }
            }

        }

        void OnlineGameplayNet::Pump()
        {
            IOnlineTransport* transport = GetTransport();
            if (!transport || !transport->IsAvailable()) {
                return;
            }

            OnlinePacket packet;
            constexpr int kMaxPacketsPerFrame = 64;
            for (int i = 0; i < kMaxPacketsPerFrame && transport->ReceivePacket(packet); ++i) {
                HandlePacket(packet);
                packet = {};
            }
        }

        bool OnlineGameplayNet::IsInOnlineLobby()
        {
            return OnlineSystem::GetInstance().IsInLobby();
        }

        bool OnlineGameplayNet::IsLobbyHost()
        {
            return IsLobbyOwner();
        }

        bool OnlineGameplayNet::IsLobbyOwner()
        {
            return OnlineSystem::GetInstance().IsLobbyOwner();
        }

        OnlineUserId OnlineGameplayNet::GetLocalUserId()
        {
            return OnlineSystem::GetInstance().GetLocalUserId();
        }

        OnlineUserId OnlineGameplayNet::GetLobbyHostUserId()
        {
            IOnlineLobby* lobby = GetLobby();
            return lobby ? lobby->GetCurrentLobby().ownerUserId : OnlineUserId();
        }

        std::vector<OnlineUserId> OnlineGameplayNet::GetOrderedLobbyMembers()
        {
            return OnlineSystem::GetInstance().GetOrderedLobbyMembers();
        }

        std::size_t OnlineGameplayNet::GetLocalPlayerIndex()
        {
            return OnlineSystem::GetInstance().GetLocalPlayerIndex();
        }

        std::size_t OnlineGameplayNet::GetRemoteLobbyMemberCount()
        {
            return GetRemoteLobbyMembers().size();
        }

        bool OnlineGameplayNet::BroadcastStartMatch(const std::string& scenePath)
        {
            return SendToRemoteLobbyMembers(
                BuildStartMatchPacket(scenePath),
                kControlChannel,
                OnlinePacketReliability::Reliable);
        }

        bool OnlineGameplayNet::ConsumeStartMatch(std::string& outScenePath)
        {
            if (pendingStartMatches.empty()) {
                return false;
            }

            outScenePath = pendingStartMatches.front();
            pendingStartMatches.pop_front();
            return true;
        }

        bool OnlineGameplayNet::SendPlayerInput(const PlayerInputSnapshot& snapshot)
        {
            if (IsLobbyHost()) {
                return false;
            }

            return SendToHost(
                BuildPlayerInputPacket(snapshot),
                kInputChannel,
                OnlinePacketReliability::Unreliable);
        }

        bool OnlineGameplayNet::TryGetLatestInputForUser(
            const std::string& ownerUserIdKey,
            PlayerInputSnapshot& outSnapshot)
        {
            const auto it = latestInputs.find(ownerUserIdKey);
            if (it == latestInputs.end()) {
                return false;
            }

            outSnapshot = it->second;
            return true;
        }

        bool OnlineGameplayNet::BroadcastTransform(const TransformSnapshot& snapshot)
        {
            return SendToRemoteLobbyMembers(
                BuildTransformPacket(snapshot),
                kSnapshotChannel,
                OnlinePacketReliability::Unreliable);
        }

        bool OnlineGameplayNet::TryGetLatestTransform(const std::string& objectKey, TransformSnapshot& outSnapshot)
        {
            const auto it = latestTransforms.find(objectKey);
            if (it == latestTransforms.end()) {
                return false;
            }

            outSnapshot = it->second;
            return true;
        }

    }
}
