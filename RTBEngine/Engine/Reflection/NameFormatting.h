#pragma once

#include "../RTBEngineAPI.h"
#include <string>

namespace RTBEngine {
    namespace Reflection {

        // Converts camelCase, PascalCase, or snake_case identifiers into Title Case labels
        // (e.g. "maxAmmo" -> "Max Ammo"). Strings that already contain spaces are returned as-is.
        RTB_API std::string FormatPropertyName(const std::string& identifier);
        RTB_API std::string FormatPropertyName(const char* identifier);

    }
}
