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

            static void SetupLuaBindings(lua_State* L);

            static ECS::GameObject* ProcessGameObject(lua_State* L, int tableIndex, ECS::Scene* scene,
                std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static ECS::GameObject* ProcessPrefabInstance(lua_State* L, int tableIndex, ECS::Scene* scene,
                const std::string& name, const std::string& uuid,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static void ProcessComponents(lua_State* L, int arrayIndex, ECS::GameObject* gameObject,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static void ReadTransform(lua_State* L, int tableIndex, ECS::GameObject* go);

            static void ProcessChildren(lua_State* L, int tableIndex, ECS::Scene* scene,
                ECS::GameObject* parent,
                std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static void ResolveParenting(ECS::Scene* scene,
                const std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests);

            static void ResolveUUIDRefs(ECS::Scene* scene,
                const std::vector<UUIDRefRequest>& uuidRefRequests);
        };

    }
}
