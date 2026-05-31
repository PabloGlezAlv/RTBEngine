#pragma once

#include "../RTBEngineAPI.h"
#include "IOnlineTransport.h"
#include "OnlineUser.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace RTBEngine {
    namespace Online {

        struct RTB_API OnlineMessageContext {
            OnlineUserId senderUserId;
            std::uint8_t channel = 0;
            const std::uint8_t* payload = nullptr;
            std::size_t payloadSize = 0;
        };

        using OnlineMessageHandler = void(*)(const OnlineMessageContext& context);

        class RTB_API OnlineMessageBus {
        public:
            static constexpr std::uint16_t kEngineMessageIdMax = 63;
            static constexpr std::uint16_t kGameMessageIdMin = 64;

            static void RegisterHandler(std::uint16_t messageId, OnlineMessageHandler handler);
            static void UnregisterHandler(std::uint16_t messageId);
            static void ClearHandlers();

            static bool SendToHost(
                std::uint16_t messageId,
                const std::vector<std::uint8_t>& payload,
                std::uint8_t channel,
                OnlinePacketReliability reliability);

            static bool BroadcastToClients(
                std::uint16_t messageId,
                const std::vector<std::uint8_t>& payload,
                std::uint8_t channel,
                OnlinePacketReliability reliability);

            static void Pump();
        };

    }
}
