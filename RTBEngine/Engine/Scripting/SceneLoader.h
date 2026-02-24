#pragma once
#include <string>
#include <lua.hpp>
#include <vector>

namespace RTBEngine {
    namespace ECS {
        class Scene;
        class GameObject;
        class Component;
    }
    namespace Reflection {
        struct PropertyInfo;
    }
}

namespace RTBEngine {
    namespace Scripting {

        class SceneLoader {
        public:
            // Load scene from Lua file
            static ECS::Scene* LoadScene(const std::string& filePath);

        private:
            struct UUIDRefRequest {
                ECS::Component* component;
                const Reflection::PropertyInfo* prop;
                std::string uuidString;
            };

            // Setup LuaBridge bindings for Math types
            static void SetupLuaBindings(lua_State* L);

            // Process a GameObject table from Lua
            static ECS::GameObject* ProcessGameObject(lua_State* L, int tableIndex, ECS::Scene* scene,
                std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);
            // Process components array for a GameObject
            static void ProcessComponents(lua_State* L, int arrayIndex, ECS::GameObject* gameObject, std::vector<UUIDRefRequest>& uuidRefRequests);
        };

    }
}
