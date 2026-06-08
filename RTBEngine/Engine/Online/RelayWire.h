#pragma once

#include "../RTBEngineAPI.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace RTBEngine {
    namespace Online {

        constexpr std::size_t kRelayIdByteLength = 16;
        constexpr std::size_t kRelayMaxDatagramSize = 1200;
        constexpr std::size_t kRelayConnectPacketSize = 4 + kRelayIdByteLength + kRelayIdByteLength;
        constexpr std::size_t kRelayGameClientHeaderSize = 4 + kRelayIdByteLength + kRelayIdByteLength;
        constexpr std::size_t kRelayGameServerHeaderSize = 4 + kRelayIdByteLength;

        RTB_API bool RelayTryHexToIdBytes(const std::string& hex, std::uint8_t* outBytes, std::size_t capacity);
        RTB_API void RelayIdBytesToHex(const std::uint8_t* idBytes, std::size_t length, std::string& outHex);
        RTB_API void RelayFillBroadcastTarget(std::uint8_t* outBytes, std::size_t capacity);
        RTB_API bool RelayIsBroadcastTarget(const std::uint8_t* targetBytes, std::size_t length);

        RTB_API std::size_t RelayWriteConnectPacket(
            std::uint8_t* buffer,
            std::size_t capacity,
            const std::uint8_t* sessionToken,
            const std::uint8_t* memberId);

        RTB_API std::size_t RelayWriteGameClientPacket(
            std::uint8_t* buffer,
            std::size_t capacity,
            const std::uint8_t* sessionToken,
            const std::uint8_t* targetMemberId,
            const void* innerPayload,
            std::size_t innerPayloadSize);

    }
}
