#pragma once

#include "../RTBEngineAPI.h"

#include <cstdint>
#include <string>
#include <vector>

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        struct RTB_API OnlineJsonMember {
            std::string memberId;
            std::string displayName;
            bool isOwner = false;
        };
#pragma warning(pop)

        RTB_API std::string NormalizeHttpBaseUrl(std::string baseUrl);
        RTB_API std::string EscapeJsonString(const std::string& value);
        RTB_API std::string ExtractJsonStringField(const std::string& json, const std::string& fieldName);
        RTB_API bool ExtractJsonBoolField(const std::string& json, const std::string& fieldName, bool defaultValue);
        RTB_API int ExtractJsonIntField(const std::string& json, const std::string& fieldName, int defaultValue);
        RTB_API std::vector<OnlineJsonMember> ExtractJsonMembers(const std::string& json);
        RTB_API std::string ExtractJsonErrorMessage(const std::string& json);

    }
}
