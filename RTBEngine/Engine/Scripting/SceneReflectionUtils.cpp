#include "SceneReflectionUtils.h"

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

#include "../Reflection/TypeInfo.h"
#include "../ECS/Component.h"
#include "../Math/Math.h"

namespace RTBEngine {
    namespace Scripting {
        namespace SceneReflectionUtils {

            using namespace Reflection;

            template<typename T>
            static void WriteValue(void* dst, const T& value) {
                *reinterpret_cast<T*>(dst) = value;
            }

            void ApplyLuaTableToComponent(lua_State* L, int tableIndex, ECS::Component* component) {
                if (!component) return;

                const Reflection::TypeInfo* typeInfo = component->GetTypeInfo();
                if (!typeInfo) return;

                const int absIndex = lua_absindex(L, tableIndex);

                for (const PropertyInfo* prop : typeInfo->GetSerializableProperties()) {
                    const char* name = prop->name.c_str();

                    lua_getfield(L, absIndex, name);
                    if (lua_isnil(L, -1)) {
                        lua_pop(L, 1);
                        continue;
                    }

                    void* dst = reinterpret_cast<char*>(component) + prop->offset;

                    switch (prop->type) {
                    case PropertyType::Bool:
                        if (lua_isboolean(L, -1)) {
                            WriteValue<bool>(dst, lua_toboolean(L, -1) != 0);
                        }
                        break;
                    case PropertyType::Int:
                    case PropertyType::Enum:
                        if (lua_isnumber(L, -1)) {
                            WriteValue<int>(dst, static_cast<int>(lua_tonumber(L, -1)));
                        }
                        break;
                    case PropertyType::Float:
                        if (lua_isnumber(L, -1)) {
                            WriteValue<float>(dst, static_cast<float>(lua_tonumber(L, -1)));
                        }
                        break;
                    case PropertyType::Double:
                        if (lua_isnumber(L, -1)) {
                            WriteValue<double>(dst, static_cast<double>(lua_tonumber(L, -1)));
                        }
                        break;
                    case PropertyType::String:
                        if (lua_isstring(L, -1)) {
                            WriteValue<std::string>(dst, std::string(lua_tostring(L, -1)));
                        }
                        break;
                    case PropertyType::Vector2:
                        if (lua_isuserdata(L, -1)) {
                            auto v2 = luabridge::Stack<Math::Vector2>::get(L, -1);
                            if (v2) WriteValue<Math::Vector2>(dst, v2.value());
                        }
                        break;
                    case PropertyType::Vector3:
                        if (lua_isuserdata(L, -1)) {
                            auto v3 = luabridge::Stack<Math::Vector3>::get(L, -1);
                            if (v3) WriteValue<Math::Vector3>(dst, v3.value());
                        }
                        break;
                    case PropertyType::Vector4:
                        if (lua_isuserdata(L, -1)) {
                            auto v4 = luabridge::Stack<Math::Vector4>::get(L, -1);
                            if (v4) WriteValue<Math::Vector4>(dst, v4.value());
                        }
                        break;
                    case PropertyType::Quaternion:
                        if (lua_isuserdata(L, -1)) {
                            auto q = luabridge::Stack<Math::Quaternion>::get(L, -1);
                            if (q) WriteValue<Math::Quaternion>(dst, q.value());
                        }
                        break;
                    case PropertyType::Color:
                        if (lua_isuserdata(L, -1)) {
                            auto c = luabridge::Stack<Math::Color>::get(L, -1);
                            if (c) WriteValue<Math::Color>(dst, c.value());
                        }
                        break;
                    default:
                        // AssetRef, TextureRef, AudioClipRef, MeshRef, FontRef, GameObjectRef, ComponentRef...
                        // These are resolved by other systems (ResourceManager, UUID resolution, etc.).
                        break;
                    }

                    lua_pop(L, 1);
                }
            }

        }
    }
}

