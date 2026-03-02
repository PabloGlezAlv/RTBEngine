#pragma once

#include <string>
#include "../Math/Math.h"

struct lua_State;

namespace RTBEngine {
    namespace Scripting {
        namespace SceneParsingUtils {

            std::string ReadOptionalString(lua_State* L, int tableIndex, const char* fieldName, const std::string& defaultValue = "");
            int ReadOptionalInt(lua_State* L, int tableIndex, const char* fieldName, int defaultValue);
            float ReadOptionalFloat(lua_State* L, int tableIndex, const char* fieldName, float defaultValue);
            bool ReadOptionalBool(lua_State* L, int tableIndex, const char* fieldName, bool defaultValue);

            Math::Vector2 ReadOptionalVector2(lua_State* L, int tableIndex, const char* fieldName, const Math::Vector2& defaultValue);
            Math::Vector3 ReadOptionalVector3(lua_State* L, int tableIndex, const char* fieldName, const Math::Vector3& defaultValue);
            Math::Vector4 ReadOptionalVector4(lua_State* L, int tableIndex, const char* fieldName, const Math::Vector4& defaultValue);
            Math::Quaternion ReadOptionalQuaternion(lua_State* L, int tableIndex, const char* fieldName, const Math::Quaternion& defaultValue);

            // Minimal structural validation for the root table returned by CreateScene().
            // Returns true if the table has at least the expected core fields.
            bool ValidateSceneTable(lua_State* L, int sceneTableIndex, const std::string& filePath);

        }
    }
}

