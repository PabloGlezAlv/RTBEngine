#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Quaternions/Quaternion.h"
#include "../Math/Vectors/Vector3.h"
#include "OnlineUser.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RTBEngine {
    namespace Online {

        class RTB_API OnlineGameplayNet {
        public:
            static constexpr std::uint16_t kMessageStartMatch = 1;
            static constexpr std::uint16_t kMessageTransformSnapshot = 2;
            static constexpr std::uint16_t kMessagePlayerInput = 3;

            struct TransformSnapshot {
                std::string objectKey;
                Math::Vector3 position = Math::Vector3::Zero();
                Math::Quaternion rotation = Math::Quaternion::Identity();
                OnlineUserId senderUserId;
            };

            struct PlayerInputSnapshot {
                OnlineUserId senderUserId;
                std::uint32_t sequenceNumber = 0;
                float moveX = 0.0f;
                float moveZ = 0.0f;
                bool sprint = false;
            };

            static void Pump();

            static bool IsInOnlineLobby();
            static bool IsLobbyHost();
            static bool IsLobbyOwner();
            static std::size_t GetRemoteLobbyMemberCount();

            static OnlineUserId GetLocalUserId();
            static OnlineUserId GetLobbyHostUserId();
            static std::vector<OnlineUserId> GetOrderedLobbyMembers();
            static std::size_t GetLocalPlayerIndex();

            static bool BroadcastStartMatch(const std::string& scenePath);
            static bool ConsumeStartMatch(std::string& outScenePath);

            static bool SendPlayerInput(const PlayerInputSnapshot& snapshot);
            static bool TryGetLatestInputForUser(
                const std::string& ownerUserIdKey,
                PlayerInputSnapshot& outSnapshot);

            static bool BroadcastTransform(const TransformSnapshot& snapshot);
            static bool TryGetLatestTransform(const std::string& objectKey, TransformSnapshot& outSnapshot);
        };

    }
}
