#include "OnlineGameplayNet.h"

#include "OnlineMessageBus.h"
#include "OnlineMessageCodec.h"
#include "IOnlineLobby.h"
#include "OnlineSystem.h"

#include "../Core/Logger.h"

#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RTBEngine {
    namespace Online {

        namespace {

            constexpr std::uint8_t kControlChannel = 0;
            constexpr std::uint8_t kSnapshotChannel = 1;
            constexpr std::uint8_t kInputChannel = 2;

            std::deque<std::string> pendingStartMatches;
            std::unordered_map<std::uint32_t, OnlineGameplayNet::TransformSnapshot> latestTransforms;
            std::unordered_map<std::string, OnlineGameplayNet::PlayerInputSnapshot> latestInputs;
            std::unordered_set<std::uint32_t> registeredNetworkObjectIds;
            std::uint32_t nextNetworkObjectId = 1;
            bool engineHandlersRegistered = false;

            std::vector<std::uint8_t> BuildStartMatchPayload(const std::string& scenePath)
            {
                std::vector<std::uint8_t> bytes;
                OnlineMessageCodec::AppendString(bytes, scenePath);
                return bytes;
            }

            std::vector<std::uint8_t> BuildTransformPayload(const OnlineGameplayNet::TransformSnapshot& snapshot)
            {
                std::vector<std::uint8_t> bytes;
                OnlineMessageCodec::AppendValue(bytes, snapshot.networkId);
                OnlineMessageCodec::AppendValue(bytes, snapshot.position.x);
                OnlineMessageCodec::AppendValue(bytes, snapshot.position.y);
                OnlineMessageCodec::AppendValue(bytes, snapshot.position.z);
                OnlineMessageCodec::AppendValue(bytes, snapshot.rotation.x);
                OnlineMessageCodec::AppendValue(bytes, snapshot.rotation.y);
                OnlineMessageCodec::AppendValue(bytes, snapshot.rotation.z);
                OnlineMessageCodec::AppendValue(bytes, snapshot.rotation.w);
                return bytes;
            }

            std::vector<std::uint8_t> BuildPlayerInputPayload(const OnlineGameplayNet::PlayerInputSnapshot& snapshot)
            {
                std::vector<std::uint8_t> bytes;
                OnlineMessageCodec::AppendValue(bytes, snapshot.sequenceNumber);
                OnlineMessageCodec::AppendValue(bytes, snapshot.moveX);
                OnlineMessageCodec::AppendValue(bytes, snapshot.moveZ);
                const std::uint8_t sprintFlag = snapshot.sprint ? 1 : 0;
                OnlineMessageCodec::AppendValue(bytes, sprintFlag);
                return bytes;
            }

            void HandleStartMatch(const OnlineMessageContext& context)
            {
                std::size_t offset = 0;
                std::string scenePath;
                if (!OnlineMessageCodec::ReadString(context.payload, context.payloadSize, offset, scenePath) ||
                    scenePath.empty()) {
                    return;
                }

                pendingStartMatches.push_back(scenePath);
                RTB_INFO("OnlineGameplayNet: received StartMatch for scene " + scenePath);
            }

            void HandleTransformSnapshot(const OnlineMessageContext& context)
            {
                std::size_t offset = 0;
                OnlineGameplayNet::TransformSnapshot snapshot;
                snapshot.senderUserId = context.senderUserId;

                if (!OnlineMessageCodec::ReadValue(context.payload, context.payloadSize, offset, snapshot.networkId) ||
                    !OnlineMessageCodec::ReadValue(context.payload, context.payloadSize, offset, snapshot.position.x) ||
                    !OnlineMessageCodec::ReadValue(context.payload, context.payloadSize, offset, snapshot.position.y) ||
                    !OnlineMessageCodec::ReadValue(context.payload, context.payloadSize, offset, snapshot.position.z) ||
                    !OnlineMessageCodec::ReadValue(context.payload, context.payloadSize, offset, snapshot.rotation.x) ||
                    !OnlineMessageCodec::ReadValue(context.payload, context.payloadSize, offset, snapshot.rotation.y) ||
                    !OnlineMessageCodec::ReadValue(context.payload, context.payloadSize, offset, snapshot.rotation.z) ||
                    !OnlineMessageCodec::ReadValue(context.payload, context.payloadSize, offset, snapshot.rotation.w) ||
                    snapshot.networkId == OnlineGameplayNet::kInvalidNetworkObjectId) {
                    return;
                }

                latestTransforms[snapshot.networkId] = snapshot;
            }

            void HandlePlayerInput(const OnlineMessageContext& context)
            {
                std::size_t offset = 0;
                OnlineGameplayNet::PlayerInputSnapshot snapshot;
                snapshot.senderUserId = context.senderUserId;
                std::uint8_t sprintFlag = 0;

                if (!OnlineMessageCodec::ReadValue(context.payload, context.payloadSize, offset, snapshot.sequenceNumber) ||
                    !OnlineMessageCodec::ReadValue(context.payload, context.payloadSize, offset, snapshot.moveX) ||
                    !OnlineMessageCodec::ReadValue(context.payload, context.payloadSize, offset, snapshot.moveZ) ||
                    !OnlineMessageCodec::ReadValue(context.payload, context.payloadSize, offset, sprintFlag)) {
                    return;
                }

                snapshot.sprint = sprintFlag != 0;
                latestInputs[snapshot.senderUserId.ToString()] = snapshot;
            }

            void RegisterEngineHandlers()
            {
                if (engineHandlersRegistered) {
                    return;
                }

                OnlineMessageBus::RegisterHandler(OnlineGameplayNet::kMessageStartMatch, &HandleStartMatch);
                OnlineMessageBus::RegisterHandler(OnlineGameplayNet::kMessageTransformSnapshot, &HandleTransformSnapshot);
                OnlineMessageBus::RegisterHandler(OnlineGameplayNet::kMessagePlayerInput, &HandlePlayerInput);
                engineHandlersRegistered = true;
            }

        }

        void OnlineGameplayNet::Pump()
        {
            RegisterEngineHandlers();
            OnlineMessageBus::Pump();
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
            IOnlineLobby* lobby = OnlineSystem::GetInstance().GetLobby();
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
            std::size_t count = 0;
            if (!IsInOnlineLobby()) {
                return count;
            }

            const std::vector<OnlineUserId> members = GetOrderedLobbyMembers();
            const OnlineUserId localUserId = GetLocalUserId();
            for (const OnlineUserId& member : members) {
                if (member.IsValid() && member != localUserId) {
                    ++count;
                }
            }

            return count;
        }

        bool OnlineGameplayNet::BroadcastStartMatch(const std::string& scenePath)
        {
            RegisterEngineHandlers();
            return OnlineMessageBus::BroadcastToClients(
                kMessageStartMatch,
                BuildStartMatchPayload(scenePath),
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

            RegisterEngineHandlers();
            return OnlineMessageBus::SendToHost(
                kMessagePlayerInput,
                BuildPlayerInputPayload(snapshot),
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

        void OnlineGameplayNet::ResetNetworkSession()
        {
            latestTransforms.clear();
            registeredNetworkObjectIds.clear();
            nextNetworkObjectId = 1;
        }

        std::uint32_t OnlineGameplayNet::AllocateNetworkObjectId()
        {
            if (!IsLobbyHost()) {
                RTB_ERROR("OnlineGameplayNet: only the lobby host can allocate network object ids.");
                return kInvalidNetworkObjectId;
            }

            if (nextNetworkObjectId == kInvalidNetworkObjectId) {
                RTB_ERROR("OnlineGameplayNet: network object id counter overflow.");
                return kInvalidNetworkObjectId;
            }

            return nextNetworkObjectId++;
        }

        bool OnlineGameplayNet::RegisterNetworkObjectId(std::uint32_t networkId)
        {
            if (networkId == kInvalidNetworkObjectId) {
                return false;
            }

            const auto [it, inserted] = registeredNetworkObjectIds.insert(networkId);
            return inserted;
        }

        void OnlineGameplayNet::UnregisterNetworkObjectId(std::uint32_t networkId)
        {
            if (networkId == kInvalidNetworkObjectId) {
                return;
            }

            registeredNetworkObjectIds.erase(networkId);
            latestTransforms.erase(networkId);
        }

        bool OnlineGameplayNet::IsNetworkObjectIdRegistered(std::uint32_t networkId)
        {
            return networkId != kInvalidNetworkObjectId &&
                registeredNetworkObjectIds.find(networkId) != registeredNetworkObjectIds.end();
        }

        bool OnlineGameplayNet::BroadcastTransform(const TransformSnapshot& snapshot)
        {
            if (snapshot.networkId == kInvalidNetworkObjectId) {
                RTB_ERROR("OnlineGameplayNet: cannot broadcast transform with an invalid network id.");
                return false;
            }

            if (!IsNetworkObjectIdRegistered(snapshot.networkId)) {
                RTB_ERROR(
                    "OnlineGameplayNet: cannot broadcast transform for unregistered network id " +
                    std::to_string(snapshot.networkId) + ".");
                return false;
            }

            RegisterEngineHandlers();
            return OnlineMessageBus::BroadcastToClients(
                kMessageTransformSnapshot,
                BuildTransformPayload(snapshot),
                kSnapshotChannel,
                OnlinePacketReliability::Unreliable);
        }

        bool OnlineGameplayNet::TryGetLatestTransform(std::uint32_t networkId, TransformSnapshot& outSnapshot)
        {
            const auto it = latestTransforms.find(networkId);
            if (it == latestTransforms.end()) {
                return false;
            }

            outSnapshot = it->second;
            return true;
        }

    }
}
