#include "SceneReflectionUtils.h"

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

#include "../Reflection/TypeInfo.h"
#include "../ECS/Component.h"
#include "../Math/Math.h"
#include "../Core/ResourceManager.h"
#include "../RTBEngine.h"

namespace RTBEngine {
    namespace Scripting {
        namespace SceneReflectionUtils {

            using namespace Reflection;

            template<typename T>
            static void WriteValue(void* dst, const T& value) {
                *reinterpret_cast<T*>(dst) = value;
            }

            static const Reflection::TypeInfo* ResolveComponentTypeInfo(const char* componentTypeName, ECS::Component* component) {
                if (componentTypeName && componentTypeName[0] != '\0') {
                    const Reflection::TypeInfo* registeredType =
                        Reflection::TypeRegistry::GetInstance().GetTypeInfo(componentTypeName);
                    if (registeredType) {
                        return registeredType;
                    }

                    RTB_WARN("SceneReflectionUtils: No registered TypeInfo for component type '" +
                        std::string(componentTypeName) + "'. Falling back to instance metadata.");
                }

                return component ? component->GetTypeInfo() : nullptr;
            }

            void ApplyLuaTableToComponent(lua_State* L, int tableIndex, const char* componentTypeName, ECS::Component* component) {
                if (!component) {
                    return;
                }

                const Reflection::TypeInfo* typeInfo = ResolveComponentTypeInfo(componentTypeName, component);
                if (!typeInfo) {
                    RTB_WARN("SceneReflectionUtils: Missing TypeInfo while applying Lua table to component '" +
                        std::string(componentTypeName ? componentTypeName : "<unknown>") + "'.");
                    return;
                }

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

                    void* dst = prop->GetMutableData(component);
                    if (!dst) {
                        RTB_WARN("SceneReflectionUtils: Property '" + prop->name + "' on component '" +
                            std::string(componentTypeName ? componentTypeName : "<unknown>") +
                            "' resolved to a null destination pointer.");
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
                            auto v2 = luabridge::Stack<Math::Vector2>::get(L, -1);
                            if (v2) {
                                WriteValue<Math::Vector2>(dst, v2.value());
                            }
                        }
                        break;
                    case PropertyType::Vector3:
                        if (luabridge::Stack<Math::Vector3>::isInstance(L, -1)) {
                            auto v3 = luabridge::Stack<Math::Vector3>::get(L, -1);
                            if (v3) {
                                WriteValue<Math::Vector3>(dst, v3.value());
                            }
                        }
                        break;
                    case PropertyType::Vector4:
                        if (luabridge::Stack<Math::Vector4>::isInstance(L, -1)) {
                            auto v4 = luabridge::Stack<Math::Vector4>::get(L, -1);
                            if (v4) {
                                WriteValue<Math::Vector4>(dst, v4.value());
                            }
                        }
                        break;
                    case PropertyType::Quaternion:
                        if (luabridge::Stack<Math::Quaternion>::isInstance(L, -1)) {
                            auto q = luabridge::Stack<Math::Quaternion>::get(L, -1);
                            if (q) {
                                WriteValue<Math::Quaternion>(dst, q.value());
                            }
                        }
                        break;
                    case PropertyType::Color:
                        if (luabridge::Stack<Math::Color>::isInstance(L, -1)) {
                            auto c = luabridge::Stack<Math::Color>::get(L, -1);
                            if (c) {
                                WriteValue<Math::Color>(dst, c.value());
                            }
                        }
                        else if (luabridge::Stack<Math::Vector4>::isInstance(L, -1)) {
                            auto v4 = luabridge::Stack<Math::Vector4>::get(L, -1);
                            if (v4) {
                                WriteValue<Math::Color>(dst, Math::Color(v4.value()));
                            }
                        }
                        break;
                    case PropertyType::TextureRef:
                        if (lua_isstring(L, -1)) {
                            const std::string path = lua_tostring(L, -1);
                            if (!path.empty()) {
                                auto& rm = Core::ResourceManager::GetInstance();
                                Rendering::Texture* tex = nullptr;
                                if (path.size() > 8 && path.substr(path.size() - 8) == ".texture") {
                                    tex = rm.LoadTextureAsset(path);
                                }
                                else if (componentTypeName && std::string(componentTypeName) == "MeshRenderer") {
                                    tex = rm.LoadModelTexture(path);
                                }
                                else {
                                    tex = rm.LoadTexture(path);
                                }
                                WriteValue<void*>(dst, tex);
                            }
                        }
                        break;
                    case PropertyType::MeshRef:
                        if (lua_isstring(L, -1)) {
                            const std::string path = lua_tostring(L, -1);
                            if (!path.empty()) {
                                auto* mesh = Core::ResourceManager::GetInstance().LoadModel(path);
                                WriteValue<void*>(dst, mesh);
                            }
                        }
                        break;
                    case PropertyType::AudioClipRef:
                        if (lua_isstring(L, -1)) {
                            const std::string path = lua_tostring(L, -1);
                            if (!path.empty()) {
                                auto* clip = Core::ResourceManager::GetInstance().LoadAudioClip(path);
                                WriteValue<void*>(dst, clip);
                            }
                        }
                        break;
                    default:
                        // FontRef, GameObjectRef, and ComponentRef are resolved by other systems.
                        break;
                    }

                    lua_pop(L, 1);
                }
            }

            void ApplyLuaTableToComponent(lua_State* L, int tableIndex, ECS::Component* component) {
                ApplyLuaTableToComponent(L, tableIndex, component ? component->GetTypeName() : nullptr, component);
            }

        }
    }
}
