#pragma once

#include <cstdint>
#include <string>

namespace RTBEngine {
    namespace Online {

        struct UdpEndpoint {
            std::string host;
            std::uint16_t port = 0;

            bool IsValid() const { return !host.empty() && port != 0; }
            std::string ToAddress() const;
            static bool TryParse(const std::string& value, UdpEndpoint& outEndpoint);
        };

        // Resolves an IPv4 address or hostname to a host string suitable for UDP endpoints.
        bool ResolveHostAddress(const std::string& hostAddress, std::string& outHost, std::string& outError);

        // Best-effort local IPv4 string for sharing with remote clients (empty if unavailable).
        std::string GetLocalIPv4Address();

        class UdpSocket {
        public:
            UdpSocket() = default;
            ~UdpSocket();

            UdpSocket(const UdpSocket&) = delete;
            UdpSocket& operator=(const UdpSocket&) = delete;

            bool Open(const std::string& bindHost, std::uint16_t bindPort, bool allowBroadcast, std::string& outError);
            void Close();
            bool IsOpen() const { return socketHandle != nullptr; }

            bool SendTo(
                const void* data,
                std::uint32_t size,
                const UdpEndpoint& destination,
                std::string& outError);

            bool ReceiveFrom(
                void* buffer,
                std::uint32_t bufferSize,
                std::uint32_t& outBytesRead,
                UdpEndpoint& outSender,
                std::string& outError);

        private:
            void* socketHandle = nullptr;
        };

        bool InitializeWinsock(std::string& outError);
        void ShutdownWinsock();

    }
}
