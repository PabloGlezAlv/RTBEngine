#pragma once

#include "../RTBEngineAPI.h"
#include "OnlineResult.h"
#include "OnlineUser.h"

#include <cstdint>
#include <vector>

namespace RTBEngine {
    namespace Online {

        enum class OnlinePacketReliability {
            Unreliable,
            Reliable
        };

#pragma warning(push)
#pragma warning(disable: 4251)
        struct RTB_API OnlinePacket {
            OnlineUserId senderUserId;
            std::uint8_t channel = 0;
            std::vector<std::uint8_t> payload;
        };

        class RTB_API IOnlineTransport {
        public:
            virtual ~IOnlineTransport() = default;

            virtual bool IsAvailable() const = 0;
            // LAN: NetworkPeer:host:port. Relay: RelayPeer:memberIdHex.
            virtual OnlineResult SendPacket(
                const OnlineUserId& remoteUserId,
                std::uint8_t channel,
                const void* data,
                std::uint32_t size,
                OnlinePacketReliability reliability) = 0;
            // Returns false when the receive queue is empty.
            virtual bool ReceivePacket(OnlinePacket& outPacket) = 0;
            virtual void CloseConnections() = 0;
            virtual const char* GetLastError() const = 0;

            OnlineResult SendPacket(
                const OnlineUserId& remoteUserId,
                std::uint8_t channel,
                const std::vector<std::uint8_t>& payload,
                OnlinePacketReliability reliability)
            {
                return SendPacket(
                    remoteUserId,
                    channel,
                    payload.empty() ? nullptr : payload.data(),
                    static_cast<std::uint32_t>(payload.size()),
                    reliability);
            }
        };
#pragma warning(pop)

    }
}
