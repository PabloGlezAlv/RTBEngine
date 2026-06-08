#include "OnlineJson.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace RTBEngine {
    namespace Online {

        namespace {

            std::size_t FindJsonObjectEnd(const std::string& json, std::size_t objectBegin)
            {
                if (objectBegin >= json.size() || json[objectBegin] != '{') {
                    return std::string::npos;
                }

                int depth = 0;
                for (std::size_t index = objectBegin; index < json.size(); ++index) {
                    if (json[index] == '{') {
                        ++depth;
                    } else if (json[index] == '}') {
                        --depth;
                        if (depth == 0) {
                            return index;
                        }
                    }
                }

                return std::string::npos;
            }

        }

        std::string NormalizeHttpBaseUrl(std::string baseUrl)
        {
            while (!baseUrl.empty() && baseUrl.back() == '/') {
                baseUrl.pop_back();
            }

            return baseUrl;
        }

        std::string EscapeJsonString(const std::string& value)
        {
            std::ostringstream escaped;
            for (char character : value) {
                switch (character) {
                case '\\':
                    escaped << "\\\\";
                    break;
                case '"':
                    escaped << "\\\"";
                    break;
                default:
                    escaped << character;
                    break;
                }
            }

            return escaped.str();
        }

        std::string ExtractJsonStringField(const std::string& json, const std::string& fieldName)
        {
            const std::string key = "\"" + fieldName + "\":\"";
            const std::size_t keyIndex = json.find(key);
            if (keyIndex == std::string::npos) {
                return {};
            }

            const std::size_t valueBegin = keyIndex + key.size();
            const std::size_t valueEnd = json.find('"', valueBegin);
            if (valueEnd == std::string::npos || valueEnd <= valueBegin) {
                return {};
            }

            return json.substr(valueBegin, valueEnd - valueBegin);
        }

        bool ExtractJsonBoolField(const std::string& json, const std::string& fieldName, bool defaultValue)
        {
            const std::string key = "\"" + fieldName + "\":";
            const std::size_t keyIndex = json.find(key);
            if (keyIndex == std::string::npos) {
                return defaultValue;
            }

            const std::size_t valueBegin = keyIndex + key.size();
            if (json.compare(valueBegin, 4, "true") == 0) {
                return true;
            }

            if (json.compare(valueBegin, 5, "false") == 0) {
                return false;
            }

            return defaultValue;
        }

        int ExtractJsonIntField(const std::string& json, const std::string& fieldName, int defaultValue)
        {
            const std::string key = "\"" + fieldName + "\":";
            const std::size_t keyIndex = json.find(key);
            if (keyIndex == std::string::npos) {
                return defaultValue;
            }

            const std::size_t valueBegin = keyIndex + key.size();
            const std::size_t valueEnd = json.find_first_of(",}", valueBegin);
            if (valueEnd == std::string::npos || valueEnd <= valueBegin) {
                return defaultValue;
            }

            try {
                return std::stoi(json.substr(valueBegin, valueEnd - valueBegin));
            } catch (...) {
                return defaultValue;
            }
        }

        std::vector<OnlineJsonMember> ExtractJsonMembers(const std::string& json)
        {
            std::vector<OnlineJsonMember> members;

            const std::string membersKey = "\"members\":[";
            const std::size_t arrayBegin = json.find(membersKey);
            if (arrayBegin == std::string::npos) {
                return members;
            }

            std::size_t cursor = arrayBegin + membersKey.size();
            while (cursor < json.size()) {
                while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
                    ++cursor;
                }

                if (cursor >= json.size() || json[cursor] == ']') {
                    break;
                }

                if (json[cursor] != '{') {
                    break;
                }

                const std::size_t objectEnd = FindJsonObjectEnd(json, cursor);
                if (objectEnd == std::string::npos) {
                    break;
                }

                const std::string objectJson = json.substr(cursor, objectEnd - cursor + 1);
                OnlineJsonMember member;
                member.memberId = ExtractJsonStringField(objectJson, "memberId");
                member.displayName = ExtractJsonStringField(objectJson, "displayName");
                member.isOwner = ExtractJsonBoolField(objectJson, "isOwner", false);
                if (!member.memberId.empty()) {
                    members.push_back(std::move(member));
                }

                cursor = objectEnd + 1;
                while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
                    ++cursor;
                }

                if (cursor < json.size() && json[cursor] == ',') {
                    ++cursor;
                }
            }

            return members;
        }

        std::string ExtractJsonErrorMessage(const std::string& json)
        {
            const std::string message = ExtractJsonStringField(json, "message");
            if (!message.empty()) {
                return message;
            }

            const std::string error = ExtractJsonStringField(json, "error");
            return error;
        }

    }
}
