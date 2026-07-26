#pragma once

#include "../RTBEngineAPI.h"

namespace RTBEngine::Core
{
    struct RTB_API VersionInfo
    {
        static constexpr int major = 0;
        static constexpr int minor = 11;
        static constexpr int patch = 0;
        static constexpr const char* string = "0.11.0";
    };
}
