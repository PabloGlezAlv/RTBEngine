#pragma once

struct lua_State;

namespace RTBEngine {
    namespace Reflection {
        class TypeInfo;
    }

    namespace ECS {
        class Component;
    }

    namespace Scripting {
        namespace SceneReflectionUtils {

            // Fills simple component properties using stable TypeRegistry metadata
            // for the declared component type name.
            void ApplyLuaTableToComponent(lua_State* L, int tableIndex, const char* componentTypeName, ECS::Component* component);

            // Fills simple component properties using its TypeInfo
            // from a Lua table (ints, floats, bools, strings, vectors, color, quaternion).
            void ApplyLuaTableToComponent(lua_State* L, int tableIndex, ECS::Component* component);

            // GameObjectRef / ComponentRef values are resolved later; keep pointers null until then.
            void ClearReferenceProperties(ECS::Component* component, const Reflection::TypeInfo* typeInfo = nullptr);

        }
    }
}
