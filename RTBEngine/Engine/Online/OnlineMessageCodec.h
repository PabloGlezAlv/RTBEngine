#pragma once

#include "../RTBEngineAPI.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RTBEngine {
    namespace Online {

        class RTB_API OnlineMessageCodec {
        public:
            static constexpr std::uint32_t kMagic = 0x4E425452;
            static constexpr std::uint16_t kProtocolVersion = 4;
            static constexpr std::size_t kHeaderSize = sizeof(std::uint32_t) + sizeof(std::uint16_t) +
                sizeof(std::uint16_t) + sizeof(std::uint8_t);

            static void AppendValue(std::vector<std::uint8_t>& outBytes, const void* data, std::size_t size);
            template <typename T>
            static void AppendValue(std::vector<std::uint8_t>& outBytes, const T& value)
            {
                AppendValue(outBytes, &value, sizeof(T));
            }

            static void AppendString(std::vector<std::uint8_t>& outBytes, const std::string& value);
            static void AppendHeader(std::vector<std::uint8_t>& outBytes, std::uint16_t messageId);

            static bool ReadValue(const std::uint8_t* data, std::size_t size, std::size_t& inOutOffset, void* outValue, std::size_t valueSize);
            template <typename T>
            static bool ReadValue(const std::uint8_t* data, std::size_t size, std::size_t& inOutOffset, T& outValue)
            {
                return ReadValue(data, size, inOutOffset, &outValue, sizeof(T));
            }

            static bool ReadString(const std::uint8_t* data, std::size_t size, std::size_t& inOutOffset, std::string& outValue);
            static bool ReadHeader(const std::uint8_t* data, std::size_t size, std::size_t& inOutOffset, std::uint16_t& outMessageId);

            static std::vector<std::uint8_t> BuildFramedMessage(std::uint16_t messageId, const std::vector<std::uint8_t>& payload);
        };

    }
}
