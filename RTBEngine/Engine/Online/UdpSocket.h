#pragma once

#include <cstdint>
#include <string>

namespace RTBEngine {
    namespace Online {

        struct UdpEndpoint {
            std::string host;
            std::uint16_t port = 0;

            bool IsValid() const { return !host.empty() && port != 0; }
            // Returns "host:port" used by NetworkPeer user ids.
            std::string ToAddress() const;
            static bool TryParse(const std::string& value, UdpEndpoint& outEndpoint);
        };

        // Resolves IPv4 literal or hostname to a dotted-quad string.
        bool ResolveHostAddress(const std::string& hostAddress, std::string& outHost, std::string& outError);

        // Returns the first non-loopback IPv4 address, or empty on failure.
        std::string GetLocalIPv4Address();

        class UdpSocket {
        public:
            UdpSocket() = default;
            ~UdpSocket();

            UdpSocket(const UdpSocket&) = delete;
            UdpSocket& operator=(const UdpSocket&) = delete;

            // Creates and binds a datagram socket. allowBroadcast enables SO_BROADCAST.
            bool Open(const std::string& bindHost, std::uint16_t bindPort, bool allowBroadcast, std::string& outError);
            void Close();
            bool IsOpen() const { return socketHandle != nullptr; }

            bool SendTo(
                const void* data,
                std::uint32_t size,
                const UdpEndpoint& destination,
                std::string& outError);

            // Non-blocking read; returns false when no datagram is available.
            bool ReceiveFrom(
                void* buffer,
                std::uint32_t bufferSize,
                std::uint32_t& outBytesRead,
                UdpEndpoint& outSender,
                std::string& outError);

        private:
            void* socketHandle = nullptr;
        };

        // Reference-counted Winsock startup; call once per process using UDP.
        bool InitializeWinsock(std::string& outError);
        void ShutdownWinsock();

    }
}
