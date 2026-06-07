#include "OnlineHttpClient.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winhttp.h>

#include <sstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace RTBEngine {
    namespace Online {

        namespace {

            struct ParsedUrl {
                bool useHttps = false;
                std::wstring host;
                INTERNET_PORT port = 0;
                std::wstring path;
            };

            std::wstring Utf8ToWide(const std::string& value)
            {
                if (value.empty()) {
                    return {};
                }

                const int requiredSize = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
                if (requiredSize <= 1) {
                    return {};
                }

                std::wstring wide(static_cast<std::size_t>(requiredSize - 1), L'\0');
                MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &wide[0], requiredSize);
                return wide;
            }

            bool ParseUrl(const std::string& url, ParsedUrl& outUrl, std::string& outError)
            {
                const std::wstring wideUrl = Utf8ToWide(url);
                if (wideUrl.empty()) {
                    outError = "URL is empty or invalid.";
                    return false;
                }

                URL_COMPONENTS components{};
                components.dwStructSize = sizeof(components);

                wchar_t hostBuffer[256]{};
                wchar_t pathBuffer[2048]{};

                components.lpszHostName = hostBuffer;
                components.dwHostNameLength = static_cast<DWORD>(sizeof(hostBuffer) / sizeof(hostBuffer[0]));
                components.lpszUrlPath = pathBuffer;
                components.dwUrlPathLength = static_cast<DWORD>(sizeof(pathBuffer) / sizeof(pathBuffer[0]));

                if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &components)) {
                    outError = "WinHttpCrackUrl failed.";
                    return false;
                }

                outUrl.useHttps = components.nScheme == INTERNET_SCHEME_HTTPS;
                outUrl.host.assign(components.lpszHostName, components.dwHostNameLength);
                outUrl.path.assign(components.lpszUrlPath, components.dwUrlPathLength);
                outUrl.port = components.nPort != 0
                    ? components.nPort
                    : (outUrl.useHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);

                if (outUrl.host.empty()) {
                    outError = "URL host is missing.";
                    return false;
                }

                if (outUrl.path.empty()) {
                    outUrl.path = L"/";
                }

                outError.clear();
                return true;
            }

            OnlineHttpResponse SendRequest(
                const wchar_t* method,
                const std::string& url,
                const std::string& body,
                const wchar_t* contentType)
            {
                OnlineHttpResponse response;

                ParsedUrl parsedUrl;
                if (!ParseUrl(url, parsedUrl, response.error)) {
                    return response;
                }

                HINTERNET session = WinHttpOpen(
                    L"RTBEngine/1.0",
                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                    WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS,
                    0);
                if (!session) {
                    response.error = "WinHttpOpen failed.";
                    return response;
                }

                HINTERNET connection = WinHttpConnect(session, parsedUrl.host.c_str(), parsedUrl.port, 0);
                if (!connection) {
                    response.error = "WinHttpConnect failed.";
                    WinHttpCloseHandle(session);
                    return response;
                }

                DWORD requestFlags = parsedUrl.useHttps ? WINHTTP_FLAG_SECURE : 0;
                HINTERNET request = WinHttpOpenRequest(
                    connection,
                    method,
                    parsedUrl.path.c_str(),
                    nullptr,
                    WINHTTP_NO_REFERER,
                    WINHTTP_DEFAULT_ACCEPT_TYPES,
                    requestFlags);
                if (!request) {
                    response.error = "WinHttpOpenRequest failed.";
                    WinHttpCloseHandle(connection);
                    WinHttpCloseHandle(session);
                    return response;
                }

                LPCWSTR headers = WINHTTP_NO_ADDITIONAL_HEADERS;
                DWORD headersLength = 0;
                if (contentType != nullptr) {
                    static const wchar_t kJsonHeader[] = L"Content-Type: application/json\r\n";
                    headers = kJsonHeader;
                    headersLength = static_cast<DWORD>(-1L);
                }

                const BOOL sendOk = WinHttpSendRequest(
                    request,
                    headers,
                    headersLength,
                    body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
                    static_cast<DWORD>(body.size()),
                    static_cast<DWORD>(body.size()),
                    0);
                if (!sendOk || !WinHttpReceiveResponse(request, nullptr)) {
                    response.error = "HTTP request failed.";
                    WinHttpCloseHandle(request);
                    WinHttpCloseHandle(connection);
                    WinHttpCloseHandle(session);
                    return response;
                }

                DWORD statusCode = 0;
                DWORD statusCodeSize = sizeof(statusCode);
                if (WinHttpQueryHeaders(
                        request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &statusCode,
                        &statusCodeSize,
                        WINHTTP_NO_HEADER_INDEX)) {
                    response.statusCode = static_cast<long>(statusCode);
                }

                std::vector<char> chunk(4096);
                while (true) {
                    DWORD bytesAvailable = 0;
                    if (!WinHttpQueryDataAvailable(request, &bytesAvailable)) {
                        break;
                    }

                    if (bytesAvailable == 0) {
                        break;
                    }

                    if (chunk.size() < bytesAvailable) {
                        chunk.resize(bytesAvailable);
                    }

                    DWORD bytesRead = 0;
                    if (!WinHttpReadData(request, chunk.data(), bytesAvailable, &bytesRead)) {
                        break;
                    }

                    if (bytesRead == 0) {
                        break;
                    }

                    response.body.append(chunk.data(), chunk.data() + bytesRead);
                }

                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connection);
                WinHttpCloseHandle(session);

                response.success = response.statusCode >= 200 && response.statusCode < 300;
                if (!response.success && response.error.empty()) {
                    response.error = "HTTP status " + std::to_string(response.statusCode);
                }

                return response;
            }

        }

        OnlineHttpResponse OnlineHttpClient::Get(const std::string& url)
        {
            return SendRequest(L"GET", url, {}, nullptr);
        }

        OnlineHttpResponse OnlineHttpClient::PostJson(const std::string& url, const std::string& jsonBody)
        {
            return SendRequest(L"POST", url, jsonBody, L"application/json");
        }

    }
}
