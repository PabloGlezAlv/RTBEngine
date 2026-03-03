#include "SceneParsingUtils.h"

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

#include "../Math/Color.h"
#include "../RTBEngine.h"

namespace RTBEngine {
    namespace Scripting {
        namespace SceneParsingUtils {

            std::string ReadOptionalString(lua_State* L, int tableIndex, const char* fieldName, const std::string& defaultValue) {
                lua_getfield(L, tableIndex, fieldName);
                std::string result = defaultValue;
                if (lua_isstring(L, -1)) {
                    result = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                return result;
            }

            int ReadOptionalInt(lua_State* L, int tableIndex, const char* fieldName, int defaultValue) {
                lua_getfield(L, tableIndex, fieldName);
                int result = defaultValue;
                if (lua_isnumber(L, -1)) {
                    result = static_cast<int>(lua_tonumber(L, -1));
                }
                lua_pop(L, 1);
                return result;
            }

            float ReadOptionalFloat(lua_State* L, int tableIndex, const char* fieldName, float defaultValue) {
                lua_getfield(L, tableIndex, fieldName);
                float result = defaultValue;
                if (lua_isnumber(L, -1)) {
                    result = static_cast<float>(lua_tonumber(L, -1));
                }
                lua_pop(L, 1);
                return result;
            }

            bool ReadOptionalBool(lua_State* L, int tableIndex, const char* fieldName, bool defaultValue) {
                lua_getfield(L, tableIndex, fieldName);
                bool result = defaultValue;
                if (lua_isboolean(L, -1)) {
                    result = lua_toboolean(L, -1) != 0;
                }
                lua_pop(L, 1);
                return result;
            }

            Math::Vector3 ReadOptionalVector3(lua_State* L, int tableIndex, const char* fieldName, const Math::Vector3& defaultValue) {
                lua_getfield(L, tableIndex, fieldName);
                Math::Vector3 result = defaultValue;
                if (lua_isuserdata(L, -1)) {
                    auto vecResult = luabridge::Stack<Math::Vector3>::get(L, -1);
                    if (vecResult) {
                        result = vecResult.value();
                    }
                }
                lua_pop(L, 1);
                return result;
            }

            Math::Vector2 ReadOptionalVector2(lua_State* L, int tableIndex, const char* fieldName, const Math::Vector2& defaultValue) {
                lua_getfield(L, tableIndex, fieldName);
                Math::Vector2 result = defaultValue;
                if (lua_isuserdata(L, -1)) {
                    auto vecResult = luabridge::Stack<Math::Vector2>::get(L, -1);
                    if (vecResult) {
                        result = vecResult.value();
                    }
                }
                lua_pop(L, 1);
                return result;
            }

            Math::Vector4 ReadOptionalVector4(lua_State* L, int tableIndex, const char* fieldName, const Math::Vector4& defaultValue) {
                lua_getfield(L, tableIndex, fieldName);
                Math::Vector4 result = defaultValue;
                if (lua_isuserdata(L, -1)) {
                    if (luabridge::Stack<Math::Vector4>::isInstance(L, -1)) {
                        auto vecResult = luabridge::Stack<Math::Vector4>::get(L, -1);
                        if (vecResult) result = vecResult.value();
                    } else if (luabridge::Stack<Math::Color>::isInstance(L, -1)) {
                        // Scene files serialized with SceneSaver use Color(...) for color fields
                        auto colorResult = luabridge::Stack<Math::Color>::get(L, -1);
                        if (colorResult) {
                            const Math::Color& c = colorResult.value();
                            result = Math::Vector4(c.r, c.g, c.b, c.a);
                        }
                    }
                }
                lua_pop(L, 1);
                return result;
            }

            Math::Quaternion ReadOptionalQuaternion(lua_State* L, int tableIndex, const char* fieldName, const Math::Quaternion& defaultValue) {
                lua_getfield(L, tableIndex, fieldName);
                Math::Quaternion result = defaultValue;
                if (lua_isuserdata(L, -1)) {
                    auto quatResult = luabridge::Stack<Math::Quaternion>::get(L, -1);
                    if (quatResult) {
                        result = quatResult.value();
                    }
                }
                lua_pop(L, 1);
                return result;
            }

            bool ValidateSceneTable(lua_State* L, int sceneTableIndex, const std::string& filePath) {
                const int absSceneTableIndex = lua_absindex(L, sceneTableIndex);
                if (!lua_istable(L, absSceneTableIndex)) {
                    RTB_ERROR("SceneLoader: CreateScene() did not return a table (" + filePath + ")");
                    return false;
                }

                lua_getfield(L, absSceneTableIndex, "gameObjects");
                const bool ok = lua_istable(L, -1);
                lua_pop(L, 1);

                if (!ok) {
                    RTB_ERROR("SceneLoader: Invalid scene table: missing 'gameObjects' array (" + filePath + ")");
                    return false;
                }

                // Optional: 'version' is allowed but not required yet. We only validate its type if present.
                lua_getfield(L, absSceneTableIndex, "version");
                if (!lua_isnil(L, -1) && !lua_isnumber(L, -1)) {
                    RTB_WARN("SceneLoader: Scene 'version' should be a number (" + filePath + ")");
                }
                lua_pop(L, 1);

                return true;
            }

        }
    }
}

