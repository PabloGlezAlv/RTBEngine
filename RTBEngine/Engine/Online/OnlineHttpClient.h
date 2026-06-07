#pragma once

#include "../RTBEngineAPI.h"

#include <cstdint>
#include <string>

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        struct RTB_API OnlineHttpResponse {
            long statusCode = 0;
            std::string body;
            std::string error;
            bool success = false;
        };

        class RTB_API OnlineHttpClient {
        public:
            static OnlineHttpResponse Get(const std::string& url);
            static OnlineHttpResponse PostJson(const std::string& url, const std::string& jsonBody);
        };
#pragma warning(pop)

    }
}
