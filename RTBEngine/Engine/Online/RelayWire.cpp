#include "RelayWire.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace RTBEngine {
    namespace Online {

        namespace {

            bool TryParseHexNibble(char character, std::uint8_t& outValue)
            {
                if (character >= '0' && character <= '9') {
                    outValue = static_cast<std::uint8_t>(character - '0');
                    return true;
                }

                const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                if (lower >= 'a' && lower <= 'f') {
                    outValue = static_cast<std::uint8_t>(10 + (lower - 'a'));
                    return true;
                }

                return false;
            }

        }

        bool RelayTryHexToIdBytes(const std::string& hex, std::uint8_t* outBytes, std::size_t capacity)
        {
            if (!outBytes || capacity < kRelayIdByteLength || hex.size() != kRelayIdByteLength * 2) {
                return false;
            }

            for (std::size_t index = 0; index < kRelayIdByteLength; ++index) {
                std::uint8_t highNibble = 0;
                std::uint8_t lowNibble = 0;
                if (!TryParseHexNibble(hex[index * 2], highNibble) ||
                    !TryParseHexNibble(hex[index * 2 + 1], lowNibble)) {
                    return false;
                }

                outBytes[index] = static_cast<std::uint8_t>((highNibble << 4) | lowNibble);
            }

            return true;
        }

        void RelayIdBytesToHex(const std::uint8_t* idBytes, std::size_t length, std::string& outHex)
        {
            static constexpr char kHexDigits[] = "0123456789abcdef";
            outHex.clear();
            outHex.reserve(length * 2);

            for (std::size_t index = 0; index < length; ++index) {
                const std::uint8_t value = idBytes[index];
                outHex.push_back(kHexDigits[(value >> 4) & 0x0F]);
                outHex.push_back(kHexDigits[value & 0x0F]);
            }
        }

        void RelayFillBroadcastTarget(std::uint8_t* outBytes, std::size_t capacity)
        {
            if (!outBytes || capacity < kRelayIdByteLength) {
                return;
            }

            std::memset(outBytes, 0xFF, kRelayIdByteLength);
        }

        bool RelayIsBroadcastTarget(const std::uint8_t* targetBytes, std::size_t length)
        {
            if (!targetBytes || length < kRelayIdByteLength) {
                return false;
            }

            for (std::size_t index = 0; index < kRelayIdByteLength; ++index) {
                if (targetBytes[index] != 0xFF) {
                    return false;
                }
            }

            return true;
        }

        std::size_t RelayWriteConnectPacket(
            std::uint8_t* buffer,
            std::size_t capacity,
            const std::uint8_t* sessionToken,
            const std::uint8_t* memberId)
        {
            if (!buffer || capacity < kRelayConnectPacketSize || !sessionToken || !memberId) {
                return 0;
            }

            std::memcpy(buffer, "RTBC", 4);
            std::memcpy(buffer + 4, sessionToken, kRelayIdByteLength);
            std::memcpy(buffer + 4 + kRelayIdByteLength, memberId, kRelayIdByteLength);
            return kRelayConnectPacketSize;
        }

        std::size_t RelayWriteGameClientPacket(
            std::uint8_t* buffer,
            std::size_t capacity,
            const std::uint8_t* sessionToken,
            const std::uint8_t* targetMemberId,
            const void* innerPayload,
            std::size_t innerPayloadSize)
        {
            const std::size_t totalSize = kRelayGameClientHeaderSize + innerPayloadSize;
            if (!buffer || capacity < totalSize || totalSize > kRelayMaxDatagramSize ||
                !sessionToken || !targetMemberId || !innerPayload || innerPayloadSize == 0) {
                return 0;
            }

            std::memcpy(buffer, "RTBG", 4);
            std::memcpy(buffer + 4, sessionToken, kRelayIdByteLength);
            std::memcpy(buffer + 4 + kRelayIdByteLength, targetMemberId, kRelayIdByteLength);
            std::memcpy(buffer + kRelayGameClientHeaderSize, innerPayload, innerPayloadSize);
            return totalSize;
        }

    }
}
