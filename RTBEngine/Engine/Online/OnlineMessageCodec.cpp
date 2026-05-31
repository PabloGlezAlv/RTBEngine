#include "OnlineMessageCodec.h"

#include <algorithm>
#include <cstring>

namespace RTBEngine {
    namespace Online {

        void OnlineMessageCodec::AppendValue(std::vector<std::uint8_t>& outBytes, const void* data, std::size_t size)
        {
            const auto* raw = static_cast<const std::uint8_t*>(data);
            outBytes.insert(outBytes.end(), raw, raw + size);
        }

        void OnlineMessageCodec::AppendString(std::vector<std::uint8_t>& outBytes, const std::string& value)
        {
            const std::uint16_t length =
                static_cast<std::uint16_t>(std::min<std::size_t>(value.size(), 1024));
            AppendValue(outBytes, length);
            outBytes.insert(outBytes.end(), value.begin(), value.begin() + length);
        }

        void OnlineMessageCodec::AppendHeader(std::vector<std::uint8_t>& outBytes, std::uint16_t messageId)
        {
            AppendValue(outBytes, kMagic);
            AppendValue(outBytes, kProtocolVersion);
            AppendValue(outBytes, messageId);
            const std::uint8_t reserved = 0;
            AppendValue(outBytes, reserved);
        }

        bool OnlineMessageCodec::ReadValue(
            const std::uint8_t* data,
            std::size_t size,
            std::size_t& inOutOffset,
            void* outValue,
            std::size_t valueSize)
        {
            if (inOutOffset + valueSize > size) {
                return false;
            }

            std::memcpy(outValue, data + inOutOffset, valueSize);
            inOutOffset += valueSize;
            return true;
        }

        bool OnlineMessageCodec::ReadString(
            const std::uint8_t* data,
            std::size_t size,
            std::size_t& inOutOffset,
            std::string& outValue)
        {
            std::uint16_t length = 0;
            if (!ReadValue(data, size, inOutOffset, length) || inOutOffset + length > size) {
                return false;
            }

            outValue.assign(reinterpret_cast<const char*>(data + inOutOffset), static_cast<std::size_t>(length));
            inOutOffset += length;
            return true;
        }

        bool OnlineMessageCodec::ReadHeader(
            const std::uint8_t* data,
            std::size_t size,
            std::size_t& inOutOffset,
            std::uint16_t& outMessageId)
        {
            std::uint32_t magic = 0;
            std::uint16_t version = 0;
            std::uint8_t reserved = 0;

            if (!ReadValue(data, size, inOutOffset, magic) ||
                !ReadValue(data, size, inOutOffset, version) ||
                !ReadValue(data, size, inOutOffset, outMessageId) ||
                !ReadValue(data, size, inOutOffset, reserved)) {
                return false;
            }

            return magic == kMagic && version == kProtocolVersion;
        }

        std::vector<std::uint8_t> OnlineMessageCodec::BuildFramedMessage(
            std::uint16_t messageId,
            const std::vector<std::uint8_t>& payload)
        {
            std::vector<std::uint8_t> bytes;
            bytes.reserve(kHeaderSize + payload.size());
            AppendHeader(bytes, messageId);
            bytes.insert(bytes.end(), payload.begin(), payload.end());
            return bytes;
        }

    }
}
