#pragma once
#include "../RTBEngineAPI.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <unordered_map>


namespace RTBEngine {
    namespace ECS {

        class GameObject;
        class Component;

        struct RTB_API ComponentSnapshot {
            std::string typeName;
            std::vector<uint8_t> rawData;
            std::unordered_map<size_t, std::string> stringData;
            std::unordered_map<size_t, std::string> ptrPathData;
        };

        class RTB_API Prefab {
        public:
            Prefab(const std::string& name = "Prefab");
            ~Prefab();

            Prefab(const Prefab&) = delete;
            Prefab& operator=(const Prefab&) = delete;

            void AddSnapshot(ComponentSnapshot&& snap) { componentSnapshots.push_back(std::move(snap)); }

            //Hierarchy
            static std::unique_ptr<Prefab> CreateFromGameObject(const GameObject* source);
            // Instantiates the root GameObject and all its children recursively.
            // outChildren receives every child GO in scene-flat order so the caller
            // can add them to the scene (same pattern as SceneLoader::ProcessChildren).
            GameObject* Instantiate(GameObject* parent, std::vector<GameObject*>& outChildren) const;
            // Convenience overload for flat prefabs (no children).
            GameObject* Instantiate(GameObject* parent = nullptr) const;

            const std::string& GetName() const { return name; }
            const std::vector<ComponentSnapshot>& GetSnapshots() const { return componentSnapshots; }
            const std::vector<std::unique_ptr<Prefab>>& GetChildPrefabs() const { return childPrefabs; }

            static void ApplySnapshot(Component* target, const ComponentSnapshot& snap);

            static void SnapshotComponent(ComponentSnapshot& snap, const Component* comp);
        private:
            std::string name;
            std::vector<ComponentSnapshot> componentSnapshots;
            std::vector<std::unique_ptr<Prefab>> childPrefabs;
        };

    }
}
