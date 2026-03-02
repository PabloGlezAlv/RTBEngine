#include "SceneLuaBindings.h"

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

#include "../Math/Math.h"

namespace RTBEngine {
    namespace Scripting {
        namespace SceneLuaBindings {

            static Math::Quaternion QuaternionFromEulerAngles(float pitch, float yaw, float roll) {
                const float toRadians = 3.14159265f / 180.0f;
                return Math::Quaternion::FromEulerAngles(
                    pitch * toRadians,
                    yaw * toRadians,
                    roll * toRadians
                );
            }

            void SetupLuaBindings(lua_State* L) {
                luabridge::getGlobalNamespace(L)
                    .beginClass<Math::Vector3>("Vector3")
                    .addConstructor<void(*)(float, float, float)>()
                    .addProperty("x", &Math::Vector3::x)
                    .addProperty("y", &Math::Vector3::y)
                    .addProperty("z", &Math::Vector3::z)
                    .endClass();

                luabridge::getGlobalNamespace(L)
                    .beginClass<Math::Vector2>("Vector2")
                    .addConstructor<void(*)(float, float)>()
                    .addProperty("x", &Math::Vector2::x)
                    .addProperty("y", &Math::Vector2::y)
                    .endClass();

                luabridge::getGlobalNamespace(L)
                    .beginClass<Math::Quaternion>("Quaternion")
                    .addStaticFunction("FromEulerAngles", QuaternionFromEulerAngles)
                    .endClass();

                luabridge::getGlobalNamespace(L)
                    .beginClass<Math::Vector4>("Vector4")
                    .addConstructor<void(*)(float, float, float, float)>()
                    .addProperty("x", &Math::Vector4::x)
                    .addProperty("y", &Math::Vector4::y)
                    .addProperty("z", &Math::Vector4::z)
                    .addProperty("w", &Math::Vector4::w)
                    .endClass();

                luabridge::getGlobalNamespace(L)
                    .beginClass<Math::Color>("Color")
                    .addConstructor<void(*)(float, float, float, float)>()
                    .addProperty("r", &Math::Color::r)
                    .addProperty("g", &Math::Color::g)
                    .addProperty("b", &Math::Color::b)
                    .addProperty("a", &Math::Color::a)
                    .endClass();
            }

        }
    }
}

