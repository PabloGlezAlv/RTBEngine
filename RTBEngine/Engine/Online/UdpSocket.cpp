#include "UdpSocket.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#pragma comment(lib, "Ws2_32.lib")

namespace RTBEngine {
    namespace Online {

        namespace {

            int g_winsockUsers = 0;

            SOCKET ToSocket(void* handle)
            {
                return reinterpret_cast<SOCKET>(handle);
            }

            void* FromSocket(SOCKET socket)
            {
                return reinterpret_cast<void*>(socket);
            }

        }

        std::string UdpEndpoint::ToAddress() const
        {
            return host + ":" + std::to_string(port);
        }

        bool ResolveHostAddress(const std::string& hostAddress, std::string& outHost, std::string& outError)
        {
            outHost.clear();
            if (hostAddress.empty()) {
                outError = "Host address is empty.";
                return false;
            }

            std::string trimmed = hostAddress;
            while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front())) != 0) {
                trimmed.erase(trimmed.begin());
            }
            while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back())) != 0) {
                trimmed.pop_back();
            }

            if (trimmed.empty()) {
                outError = "Host address is empty.";
                return false;
            }

            in_addr ipv4Address{};
            if (inet_pton(AF_INET, trimmed.c_str(), &ipv4Address) == 1) {
                char buffer[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &ipv4Address, buffer, sizeof(buffer));
                outHost = buffer;
                outError.clear();
                return true;
            }

            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;

            addrinfo* results = nullptr;
            if (getaddrinfo(trimmed.c_str(), nullptr, &hints, &results) != 0 || !results) {
                outError = "Could not resolve host address: " + trimmed;
                return false;
            }

            for (addrinfo* entry = results; entry != nullptr; entry = entry->ai_next) {
                if (!entry->ai_addr || entry->ai_addr->sa_family != AF_INET) {
                    continue;
                }

                const auto* sockaddr = reinterpret_cast<sockaddr_in*>(entry->ai_addr);
                char buffer[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &sockaddr->sin_addr, buffer, sizeof(buffer));
                outHost = buffer;
                break;
            }

            freeaddrinfo(results);

            if (outHost.empty()) {
                outError = "Could not resolve host address: " + trimmed;
                return false;
            }

            outError.clear();
            return true;
        }

        std::string GetLocalIPv4Address()
        {
            std::string winsockError;
            if (!InitializeWinsock(winsockError)) {
                return {};
            }

            char hostName[256] = {};
            if (gethostname(hostName, sizeof(hostName)) != 0) {
                return {};
            }

            std::string resolvedHost;
            std::string resolveError;
            if (!ResolveHostAddress(hostName, resolvedHost, resolveError)) {
                return {};
            }

            if (resolvedHost == "127.0.0.1") {
                return {};
            }

            return resolvedHost;
        }

        bool UdpEndpoint::TryParse(const std::string& value, UdpEndpoint& outEndpoint)
        {
            const std::size_t separator = value.rfind(':');
            if (separator == std::string::npos || separator == 0 || separator + 1 >= value.size()) {
                return false;
            }

            try {
                const int portValue = std::stoi(value.substr(separator + 1));
                if (portValue <= 0 || portValue > 65535) {
                    return false;
                }

                outEndpoint.host = value.substr(0, separator);
                outEndpoint.port = static_cast<std::uint16_t>(portValue);
                return outEndpoint.IsValid();
            }
            catch (...) {
                return false;
            }
        }

        bool InitializeWinsock(std::string& outError)
        {
            if (g_winsockUsers > 0) {
                ++g_winsockUsers;
                return true;
            }

            WSADATA wsaData{};
            const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                outError = "WSAStartup failed with code " + std::to_string(result);
                return false;
            }

            ++g_winsockUsers;
            outError.clear();
            return true;
        }

        void ShutdownWinsock()
        {
            if (g_winsockUsers <= 0) {
                return;
            }

            --g_winsockUsers;
            if (g_winsockUsers == 0) {
                WSACleanup();
            }
        }

        UdpSocket::~UdpSocket()
        {
            Close();
        }

        bool UdpSocket::Open(const std::string& bindHost, std::uint16_t bindPort, bool allowBroadcast, std::string& outError)
        {
            Close();

            SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (socket == INVALID_SOCKET) {
                outError = "Failed to create UDP socket.";
                return false;
            }

            BOOL reuseAddress = TRUE;
            setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuseAddress), sizeof(reuseAddress));

            if (allowBroadcast) {
                BOOL broadcast = TRUE;
                setsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));
            }

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(bindPort);
            if (bindHost.empty() || bindHost == "0.0.0.0" || bindHost == "*") {
                address.sin_addr.s_addr = htonl(INADDR_ANY);
            }
            else {
                if (inet_pton(AF_INET, bindHost.c_str(), &address.sin_addr) != 1) {
                    closesocket(socket);
                    outError = "Invalid bind host: " + bindHost;
                    return false;
                }
            }

            if (bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
                outError = "Failed to bind UDP socket on port " + std::to_string(bindPort) + ".";
                closesocket(socket);
                return false;
            }

            u_long nonBlocking = 1;
            ioctlsocket(socket, FIONBIO, &nonBlocking);

            socketHandle = FromSocket(socket);
            outError.clear();
            return true;
        }

        void UdpSocket::Close()
        {
            if (!socketHandle) {
                return;
            }

            closesocket(ToSocket(socketHandle));
            socketHandle = nullptr;
        }

        bool UdpSocket::SendTo(
            const void* data,
            std::uint32_t size,
            const UdpEndpoint& destination,
            std::string& outError)
        {
            if (!socketHandle || !data || size == 0 || !destination.IsValid()) {
                outError = "Invalid UDP send request.";
                return false;
            }

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(destination.port);
            if (inet_pton(AF_INET, destination.host.c_str(), &address.sin_addr) != 1) {
                outError = "Invalid destination host: " + destination.host;
                return false;
            }

            const int sent = sendto(
                ToSocket(socketHandle),
                static_cast<const char*>(data),
                static_cast<int>(size),
                0,
                reinterpret_cast<sockaddr*>(&address),
                sizeof(address));

            if (sent == SOCKET_ERROR) {
                outError = "UDP send failed.";
                return false;
            }

            outError.clear();
            return true;
        }

        bool UdpSocket::ReceiveFrom(
            void* buffer,
            std::uint32_t bufferSize,
            std::uint32_t& outBytesRead,
            UdpEndpoint& outSender,
            std::string& outError)
        {
            outBytesRead = 0;
            if (!socketHandle || !buffer || bufferSize == 0) {
                return false;
            }

            sockaddr_in sender{};
            int senderLength = sizeof(sender);
            const int received = recvfrom(
                ToSocket(socketHandle),
                static_cast<char*>(buffer),
                static_cast<int>(bufferSize),
                0,
                reinterpret_cast<sockaddr*>(&sender),
                &senderLength);

            if (received == SOCKET_ERROR) {
                const int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    return false;
                }

                outError = "UDP receive failed.";
                return false;
            }

            if (received <= 0) {
                return false;
            }

            char hostBuffer[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &sender.sin_addr, hostBuffer, sizeof(hostBuffer));
            outSender.host = hostBuffer;
            outSender.port = ntohs(sender.sin_port);
            outBytesRead = static_cast<std::uint32_t>(received);
            outError.clear();
            return true;
        }

    }
}
