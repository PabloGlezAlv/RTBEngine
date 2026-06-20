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
            // Load scene from Lua file (deserialize + wire only; lifecycle runs in SceneManager).
            static ECS::Scene* LoadScene(const std::string& filePath);

            // Post-deserialize content that needs an active scene (FBX hierarchies, bone GOs).
            static void RebuildFbxHierarchies(ECS::Scene* scene);

        private:
            struct UUIDRefRequest {
                ECS::Component* component;
                const Reflection::PropertyInfo* prop;
                std::string uuidString;
                int listIndex = -1;
            };

            static void SetupLuaBindings(lua_State* L);

            static ECS::GameObject* ProcessGameObject(lua_State* L, int tableIndex, ECS::Scene* scene,
                std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static ECS::GameObject* ProcessPrefabInstance(lua_State* L, int tableIndex, ECS::Scene* scene,
                const std::string& name, const std::string& uuid,
                std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static void ProcessComponents(lua_State* L, int arrayIndex, ECS::GameObject* gameObject,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static void ReadTransform(lua_State* L, int tableIndex, ECS::GameObject* go);

            static void ProcessChildren(lua_State* L, int tableIndex, ECS::Scene* scene,
                ECS::GameObject* parent,
                std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static void MergeSceneChildren(lua_State* L, int tableIndex, ECS::Scene* scene,
                ECS::GameObject* parent,
                std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static void ApplySceneGameObjectFromTable(lua_State* L, int tableIndex, ECS::Scene* scene,
                ECS::GameObject* go,
                std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static ECS::GameObject* FindDirectChild(ECS::GameObject* parent,
                const std::string& uuid, const std::string& name);

            static void ResolveParenting(ECS::Scene* scene,
                const std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests);

            static void ResolveUUIDRefs(ECS::Scene* scene,
                const std::vector<UUIDRefRequest>& uuidRefRequests);
        };

    }
}
