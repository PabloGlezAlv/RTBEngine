#include "UdpSocket.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#pragma comment(lib, "Ws2_32.lib") // link Winsock automatically in MSVC builds

namespace RTBEngine {
    namespace Online {

        namespace {

            // how many engine subsystems currently require Winsock; WSACleanup only at zero
            int g_winsockUsers = 0;

            // cast stored void* back to native Winsock SOCKET handle
            SOCKET ToSocket(void* handle)
            {
                return reinterpret_cast<SOCKET>(handle);
            }

            // hide SOCKET from header by storing as opaque void*
            void* FromSocket(SOCKET socket)
            {
                return reinterpret_cast<void*>(socket);
            }

        }

        // serializes endpoint to "192.168.0.2:27015" — format used by NetworkPeer user ids
        std::string UdpEndpoint::ToAddress() const
        {
            return host + ":" + std::to_string(port); // host is dotted IPv4, port is host byte order
        }

        // converts hostname or IPv4 literal into normalized dotted-quad string in outHost
        bool ResolveHostAddress(const std::string& hostAddress, std::string& outHost, std::string& outError)
        {
            outHost.clear(); // discard previous resolver output
            if (hostAddress.empty()) {
                outError = "Host address is empty.";
                return false; // nothing to resolve
            }

            std::string trimmed = hostAddress; // work on a copy so input param stays unchanged
            while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front())) != 0) {
                trimmed.erase(trimmed.begin()); // strip leading whitespace from UI/config input
            }
            while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back())) != 0) {
                trimmed.pop_back(); // strip trailing whitespace
            }

            if (trimmed.empty()) {
                outError = "Host address is empty.";
                return false; // string was only spaces
            }

            in_addr ipv4Address{}; // Winsock struct holding 32-bit IPv4 address
            if (inet_pton(AF_INET, trimmed.c_str(), &ipv4Address) == 1) {
                // trimmed is already dotted IPv4 — skip DNS
                char buffer[INET_ADDRSTRLEN] = {}; // 16 chars enough for "255.255.255.255"
                inet_ntop(AF_INET, &ipv4Address, buffer, sizeof(buffer)); // binary → text
                outHost = buffer;
                outError.clear();
                return true;
            }

            addrinfo hints{}; // tells getaddrinfo we want UDP/IPv4 results only
            hints.ai_family = AF_INET;       // IPv4 only (LAN backend scope)
            hints.ai_socktype = SOCK_DGRAM;  // datagram socket type
            hints.ai_protocol = IPPROTO_UDP; // UDP protocol number

            addrinfo* results = nullptr; // linked list of resolved addresses (OS allocated)
            if (getaddrinfo(trimmed.c_str(), nullptr, &hints, &results) != 0 || !results) {
                outError = "Could not resolve host address: " + trimmed;
                return false; // DNS failure or no records
            }

            for (addrinfo* entry = results; entry != nullptr; entry = entry->ai_next) {
                if (!entry->ai_addr || entry->ai_addr->sa_family != AF_INET) {
                    continue; // skip non-IPv4 entries in the chain
                }

                const auto* sockaddr = reinterpret_cast<sockaddr_in*>(entry->ai_addr);
                char buffer[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &sockaddr->sin_addr, buffer, sizeof(buffer));
                outHost = buffer; // take first usable IPv4 answer
                break;
            }

            freeaddrinfo(results); // release OS resolver memory

            if (outHost.empty()) {
                outError = "Could not resolve host address: " + trimmed;
                return false; // chain had no IPv4 nodes
            }

            outError.clear();
            return true;
        }

        // returns this machine's LAN IPv4 for display; empty if unavailable or loopback-only
        std::string GetLocalIPv4Address()
        {
            std::string winsockError;
            if (!InitializeWinsock(winsockError)) {
                return {}; // cannot query network without Winsock
            }

            char hostName[256] = {}; // buffer for gethostname (machine name, not IP yet)
            if (gethostname(hostName, sizeof(hostName)) != 0) {
                return {}; // OS could not provide local hostname
            }

            std::string resolvedHost;
            std::string resolveError;
            if (!ResolveHostAddress(hostName, resolvedHost, resolveError)) {
                return {}; // hostname did not map to an address
            }

            if (resolvedHost == "127.0.0.1") {
                return {}; // loopback is not useful for LAN sharing
            }

            return resolvedHost;
        }

        // inverse of ToAddress(): splits "host:port" into UdpEndpoint fields
        bool UdpEndpoint::TryParse(const std::string& value, UdpEndpoint& outEndpoint)
        {
            const std::size_t separator = value.rfind(':'); // last colon separates port from host
            if (separator == std::string::npos || separator == 0 || separator + 1 >= value.size()) {
                return false; // missing host, missing port, or malformed string
            }

            try {
                const int portValue = std::stoi(value.substr(separator + 1)); // port after ':'
                if (portValue <= 0 || portValue > 65535) {
                    return false; // TCP/UDP port must be 1..65535
                }

                outEndpoint.host = value.substr(0, separator); // everything before last ':'
                outEndpoint.port = static_cast<std::uint16_t>(portValue);
                return outEndpoint.IsValid(); // host non-empty and port non-zero
            }
            catch (...) {
                return false; // port segment was not an integer
            }
        }

        // reference-counted WSAStartup — safe if transport and lobby both init sockets
        bool InitializeWinsock(std::string& outError)
        {
            if (g_winsockUsers > 0) {
                ++g_winsockUsers; // another subsystem joined; Winsock already up
                return true;
            }

            WSADATA wsaData{}; // filled by WSAStartup with driver details we do not use
            const int result = WSAStartup(MAKEWORD(2, 2), &wsaData); // request Winsock 2.2
            if (result != 0) {
                outError = "WSAStartup failed with code " + std::to_string(result);
                return false;
            }

            ++g_winsockUsers; // first successful startup
            outError.clear();
            return true;
        }

        // paired with InitializeWinsock; WSACleanup when last user leaves
        void ShutdownWinsock()
        {
            if (g_winsockUsers <= 0) {
                return; // already fully shut down or never started
            }

            --g_winsockUsers;
            if (g_winsockUsers == 0) {
                WSACleanup(); // release Winsock for the whole process
            }
        }

        UdpSocket::~UdpSocket()
        {
            Close(); // RAII: ensure kernel socket is released
        }

        // opens and binds a UDP socket; returns false and sets outError on any failure
        bool UdpSocket::Open(const std::string& bindHost, std::uint16_t bindPort, bool allowBroadcast, std::string& outError)
        {
            Close(); // close previous handle if Open is called twice

            SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // create UDP datagram socket
            if (socket == INVALID_SOCKET) {
                outError = "Failed to create UDP socket.";
                return false;
            }

            BOOL reuseAddress = TRUE;
            setsockopt(
                socket,
                SOL_SOCKET,
                SO_REUSEADDR,
                reinterpret_cast<const char*>(&reuseAddress),
                sizeof(reuseAddress)); // allow quick re-bind after editor restart

            if (allowBroadcast) {
                BOOL broadcast = TRUE;
                setsockopt(
                    socket,
                    SOL_SOCKET,
                    SO_BROADCAST,
                    reinterpret_cast<const char*>(&broadcast),
                    sizeof(broadcast)); // permit sends to 255.255.255.255
            }

            sockaddr_in address{}; // local bind address passed to ::bind
            address.sin_family = AF_INET;
            address.sin_port = htons(bindPort); // port in network byte order (big-endian)
            if (bindHost.empty() || bindHost == "0.0.0.0" || bindHost == "*") {
                address.sin_addr.s_addr = htonl(INADDR_ANY); // listen on every local interface
            }
            else {
                if (inet_pton(AF_INET, bindHost.c_str(), &address.sin_addr) != 1) {
                    closesocket(socket); // destroy socket before returning error
                    outError = "Invalid bind host: " + bindHost;
                    return false;
                }
            }

            if (bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
                outError = "Failed to bind UDP socket on port " + std::to_string(bindPort) + ".";
                closesocket(socket); // port may be in use by another process
                return false;
            }

            u_long nonBlocking = 1; // 1 = non-blocking mode enabled
            ioctlsocket(socket, FIONBIO, &nonBlocking); // recvfrom returns instantly when queue empty

            socketHandle = FromSocket(socket); // store opaque handle in member
            outError.clear();
            return true;
        }

        void UdpSocket::Close()
        {
            if (!socketHandle) {
                return; // already closed
            }

            closesocket(ToSocket(socketHandle)); // tell OS to release port and buffers
            socketHandle = nullptr;
        }

        // sends one datagram to destination; does not wait for delivery confirmation
        bool UdpSocket::SendTo(
            const void* data,
            std::uint32_t size,
            const UdpEndpoint& destination,
            std::string& outError)
        {
            if (!socketHandle || !data || size == 0 || !destination.IsValid()) {
                outError = "Invalid UDP send request.";
                return false; // guard against null socket or empty payload
            }

            sockaddr_in address{}; // remote address for sendto()
            address.sin_family = AF_INET;
            address.sin_port = htons(destination.port);
            if (inet_pton(AF_INET, destination.host.c_str(), &address.sin_addr) != 1) {
                outError = "Invalid destination host: " + destination.host;
                return false; // destination.host must be dotted IPv4
            }

            const int sent = sendto(
                ToSocket(socketHandle),
                static_cast<const char*>(data),
                static_cast<int>(size),
                0, // no special send flags
                reinterpret_cast<sockaddr*>(&address),
                sizeof(address)); // kernel copies bytes into one UDP datagram

            if (sent == SOCKET_ERROR) {
                outError = "UDP send failed.";
                return false; // network unreachable, buffer full, etc.
            }

            outError.clear();
            return true; // note: sent may be < size on stream sockets; for UDP usually equals size
        }

        // reads at most one datagram; false means no data (or error in outError)
        bool UdpSocket::ReceiveFrom(
            void* buffer,
            std::uint32_t bufferSize,
            std::uint32_t& outBytesRead,
            UdpEndpoint& outSender,
            std::string& outError)
        {
            outBytesRead = 0; // default when no packet consumed
            if (!socketHandle || !buffer || bufferSize == 0) {
                return false; // invalid receive target
            }

            sockaddr_in sender{}; // filled by recvfrom with source address
            int senderLength = sizeof(sender); // in/out size of sender struct
            const int received = recvfrom(
                ToSocket(socketHandle),
                static_cast<char*>(buffer),
                static_cast<int>(bufferSize),
                0, // no peek flags
                reinterpret_cast<sockaddr*>(&sender),
                &senderLength); // blocks only if socket were blocking; ours is non-blocking

            if (received == SOCKET_ERROR) {
                const int error = WSAGetLastError(); // Winsock-specific error code
                if (error == WSAEWOULDBLOCK) {
                    return false; // normal idle state — no datagram waiting
                }

                outError = "UDP receive failed.";
                return false; // real socket error
            }

            if (received <= 0) {
                return false; // zero-length datagram or graceful close (unusual for UDP)
            }

            char hostBuffer[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &sender.sin_addr, hostBuffer, sizeof(hostBuffer)); // sender IP text
            outSender.host = hostBuffer;
            outSender.port = ntohs(sender.sin_port); // sender port back to host byte order
            outBytesRead = static_cast<std::uint32_t>(received); // payload bytes written to buffer
            outError.clear();
            return true;
        }

    }
}
