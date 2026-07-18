#pragma once
#include <string>
#include <lua.hpp>
#include <vector>

namespace RTBEngine {
    namespace Scene {
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
            static Scene::Scene* LoadScene(const std::string& filePath);

            // Post-deserialize content that needs an active scene (FBX hierarchies, bone GOs).
            static void RebuildFbxHierarchies(Scene::Scene* scene);

        private:
            struct UUIDRefRequest {
                Scene::Component* component;
                const Reflection::PropertyInfo* prop;
                std::string uuidString;
                int listIndex = -1;
            };

            static void SetupLuaBindings(lua_State* L);

            static Scene::GameObject* ProcessGameObject(lua_State* L, int tableIndex, Scene::Scene* scene,
                std::vector<std::pair<Scene::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static Scene::GameObject* ProcessPrefabInstance(lua_State* L, int tableIndex, Scene::Scene* scene,
                const std::string& name, const std::string& uuid,
                std::vector<std::pair<Scene::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static void ProcessComponents(lua_State* L, int arrayIndex, Scene::GameObject* gameObject,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static void ReadTransform(lua_State* L, int tableIndex, Scene::GameObject* go);

            static void ProcessChildren(lua_State* L, int tableIndex, Scene::Scene* scene,
                Scene::GameObject* parent,
                std::vector<std::pair<Scene::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static void MergeSceneChildren(lua_State* L, int tableIndex, Scene::Scene* scene,
                Scene::GameObject* parent,
                std::vector<std::pair<Scene::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static void ApplySceneGameObjectFromTable(lua_State* L, int tableIndex, Scene::Scene* scene,
                Scene::GameObject* go,
                std::vector<std::pair<Scene::GameObject*, std::string>>& parentingRequests,
                std::vector<UUIDRefRequest>& uuidRefRequests);

            static Scene::GameObject* FindDirectChild(Scene::GameObject* parent,
                const std::string& uuid, const std::string& name);

            static void ResolveParenting(Scene::Scene* scene,
                const std::vector<std::pair<Scene::GameObject*, std::string>>& parentingRequests);

            static void ResolveUUIDRefs(Scene::Scene* scene,
                const std::vector<UUIDRefRequest>& uuidRefRequests);
        };

    }
}
