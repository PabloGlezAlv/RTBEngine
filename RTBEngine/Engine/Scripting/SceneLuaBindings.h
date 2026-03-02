#pragma once

struct lua_State;

namespace RTBEngine {
    namespace Scripting {
        namespace SceneLuaBindings {
            void SetupLuaBindings(lua_State* L);
        }
    }
}

