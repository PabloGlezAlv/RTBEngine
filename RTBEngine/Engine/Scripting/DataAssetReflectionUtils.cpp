#include "DataAssetReflectionUtils.h"

#include "../Core/Logger.h"
#include "../Data/DataAsset.h"
#include "../Math/Color.h"
#include "../Math/Math.h"
#include "../Reflection/TypeInfo.h"

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

namespace RTBEngine {
    namespace Scripting {
        namespace DataAssetReflectionUtils {

            using namespace Reflection;

            template<typename T>
            static void WriteValue(void* dst, const T& value)
            {
                *reinterpret_cast<T*>(dst) = value;
            }

            void ApplyLuaTableToDataAsset(lua_State* L, int tableIndex, Data::DataAsset* asset)
            {
                if (!asset || !L) {
                    return;
                }

                const Reflection::TypeInfo* typeInfo = asset->GetTypeInfo();
                if (!typeInfo) {
                    RTB_WARN("DataAssetReflectionUtils: Missing TypeInfo for data asset.");
                    return;
                }

                void* objectBase = asset->GetActualObject();
                const int absIndex = lua_absindex(L, tableIndex);

                for (const PropertyInfo* prop : typeInfo->GetSerializableProperties()) {
                    if (!prop) {
                        continue;
                    }

                    lua_getfield(L, absIndex, prop->name.c_str());
                    if (lua_isnil(L, -1)) {
                        lua_pop(L, 1);
                        continue;
                    }

                    void* dst = prop->GetMutableData(objectBase);
                    if (!dst) {
                        lua_pop(L, 1);
                        continue;
                    }

                    switch (prop->type) {
                    case PropertyType::Bool:
                        if (lua_isboolean(L, -1)) {
                            WriteValue<bool>(dst, lua_toboolean(L, -1) != 0);
                        }
                        break;
                    case PropertyType::Int:
                        if (lua_isnumber(L, -1)) {
                            WriteValue<int>(dst, static_cast<int>(lua_tonumber(L, -1)));
                        }
                        break;
                    case PropertyType::Enum:
                        if (lua_isnumber(L, -1)) {
                            WriteValue<int>(dst, static_cast<int>(lua_tonumber(L, -1)));
                        }
                        else if (lua_isstring(L, -1) && !prop->enumNames.empty()) {
                            const std::string enumStr = lua_tostring(L, -1);
                            for (int ei = 0; ei < static_cast<int>(prop->enumNames.size()); ++ei) {
                                if (prop->enumNames[ei] == enumStr) {
                                    WriteValue<int>(dst, ei);
                                    break;
                                }
                            }
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
                    case PropertyType::AssetRef:
                        if (lua_isstring(L, -1)) {
                            WriteValue<std::string>(dst, std::string(lua_tostring(L, -1)));
                        }
                        break;
                    case PropertyType::Vector2:
                        if (luabridge::Stack<Math::Vector2>::isInstance(L, -1)) {
                            if (auto v2 = luabridge::Stack<Math::Vector2>::get(L, -1)) {
                                WriteValue<Math::Vector2>(dst, v2.value());
                            }
                        }
                        break;
                    case PropertyType::Vector3:
                        if (luabridge::Stack<Math::Vector3>::isInstance(L, -1)) {
                            if (auto v3 = luabridge::Stack<Math::Vector3>::get(L, -1)) {
                                WriteValue<Math::Vector3>(dst, v3.value());
                            }
                        }
                        break;
                    case PropertyType::Vector4:
                        if (luabridge::Stack<Math::Vector4>::isInstance(L, -1)) {
                            if (auto v4 = luabridge::Stack<Math::Vector4>::get(L, -1)) {
                                WriteValue<Math::Vector4>(dst, v4.value());
                            }
                        }
                        break;
                    case PropertyType::Quaternion:
                        if (luabridge::Stack<Math::Quaternion>::isInstance(L, -1)) {
                            if (auto q = luabridge::Stack<Math::Quaternion>::get(L, -1)) {
                                WriteValue<Math::Quaternion>(dst, q.value());
                            }
                        }
                        break;
                    case PropertyType::Color:
                        if (luabridge::Stack<Math::Color>::isInstance(L, -1)) {
                            if (auto c = luabridge::Stack<Math::Color>::get(L, -1)) {
                                WriteValue<Math::Color>(dst, c.value());
                            }
                        }
                        else if (luabridge::Stack<Math::Vector4>::isInstance(L, -1)) {
                            if (auto v4 = luabridge::Stack<Math::Vector4>::get(L, -1)) {
                                WriteValue<Math::Color>(dst, Math::Color(v4.value()));
                            }
                        }
                        break;
                    default:
                        break;
                    }

                    lua_pop(L, 1);
                }
            }

        }
    }
}
