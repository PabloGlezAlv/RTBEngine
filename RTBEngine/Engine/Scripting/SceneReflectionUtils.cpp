#include "SceneReflectionUtils.h"

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

#include "../Reflection/TypeInfo.h"
#include "../ECS/Component.h"
#include "../Math/Math.h"
#include "../Core/ResourceManager.h"

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

                    void* dst = prop->GetMutableData(component);

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
                            for (int ei = 0; ei < (int)prop->enumNames.size(); ++ei) {
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
                        if (lua_isstring(L, -1)) {
                            WriteValue<std::string>(dst, std::string(lua_tostring(L, -1)));
                        }
                        break;
                    case PropertyType::Vector2:
                        if (luabridge::Stack<Math::Vector2>::isInstance(L, -1)) {
                            auto v2 = luabridge::Stack<Math::Vector2>::get(L, -1);
                            if (v2) WriteValue<Math::Vector2>(dst, v2.value());
                        }
                        break;
                    case PropertyType::Vector3:
                        if (luabridge::Stack<Math::Vector3>::isInstance(L, -1)) {
                            auto v3 = luabridge::Stack<Math::Vector3>::get(L, -1);
                            if (v3) WriteValue<Math::Vector3>(dst, v3.value());
                        }
                        break;
                    case PropertyType::Vector4:
                        if (luabridge::Stack<Math::Vector4>::isInstance(L, -1)) {
                            auto v4 = luabridge::Stack<Math::Vector4>::get(L, -1);
                            if (v4) WriteValue<Math::Vector4>(dst, v4.value());
                        }
                        break;
                    case PropertyType::Quaternion:
                        if (luabridge::Stack<Math::Quaternion>::isInstance(L, -1)) {
                            auto q = luabridge::Stack<Math::Quaternion>::get(L, -1);
                            if (q) WriteValue<Math::Quaternion>(dst, q.value());
                        }
                        break;
                    case PropertyType::Color:
                        if (luabridge::Stack<Math::Color>::isInstance(L, -1)) {
                            auto c = luabridge::Stack<Math::Color>::get(L, -1);
                            if (c) WriteValue<Math::Color>(dst, c.value());
                        } else if (luabridge::Stack<Math::Vector4>::isInstance(L, -1)) {
                            // SceneSaver serializes Color as Vector4 — accept both
                            auto v4 = luabridge::Stack<Math::Vector4>::get(L, -1);
                            if (v4) WriteValue<Math::Color>(dst, Math::Color(v4.value()));
                        }
                        break;
                    case PropertyType::TextureRef: {
                        if (lua_isstring(L, -1)) {
                            const std::string path = lua_tostring(L, -1);
                            if (!path.empty()) {
                                // .texture assets carry flip metadata; raw images use default flip
                                auto& rm = Core::ResourceManager::GetInstance();
                                auto* tex = (path.size() > 8 && path.substr(path.size() - 8) == ".texture")
                                    ? rm.LoadTextureAsset(path)
                                    : rm.LoadTexture(path);
                                WriteValue<void*>(dst, tex);
                            }
                        }
                        break;
                    }
                    case PropertyType::MeshRef: {
                        if (lua_isstring(L, -1)) {
                            const std::string path = lua_tostring(L, -1);
                            if (!path.empty()) {
                                auto* mesh = Core::ResourceManager::GetInstance().LoadModel(path);
                                WriteValue<void*>(dst, mesh);
                            }
                        }
                        break;
                    }
                    case PropertyType::AudioClipRef: {
                        if (lua_isstring(L, -1)) {
                            const std::string path = lua_tostring(L, -1);
                            if (!path.empty()) {
                                auto* clip = Core::ResourceManager::GetInstance().LoadAudioClip(path);
                                WriteValue<void*>(dst, clip);
                            }
                        }
                        break;
                    }
                    default:
                        // FontRef, GameObjectRef, ComponentRef are resolved by other systems.
                        break;
                    }

                    lua_pop(L, 1);
                }
            }

        }
    }
}

