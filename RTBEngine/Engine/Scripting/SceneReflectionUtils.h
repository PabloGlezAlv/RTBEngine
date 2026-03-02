#pragma once

struct lua_State;

namespace RTBEngine {
    namespace ECS {
        class Component;
    }

    namespace Scripting {
        namespace SceneReflectionUtils {

            // Fills simple component properties using its TypeInfo
            // from a Lua table (ints, floats, bools, strings, vectors, color, quaternion).
            void ApplyLuaTableToComponent(lua_State* L, int tableIndex, ECS::Component* component);

        }
    }
}

