#pragma once
#include "../RTBEngineAPI.h"
#include "../Math/Math.h"
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
            void AddChildPrefab(std::unique_ptr<Prefab> child) { childPrefabs.push_back(std::move(child)); }

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

            //Transform
            void SetPosition(const Math::Vector3& pos) { position = pos; }
            void SetRotation(const Math::Quaternion& rot) { rotation = rot; }
            void SetScale(const Math::Vector3& scl) { scale = scl; }
            const Math::Vector3& GetPosition() const { return position; }
            const Math::Quaternion& GetRotation() const { return rotation; }
            const Math::Vector3& GetScale() const { return scale; }

            int GetCollisionLayer() const { return collisionLayer; }
            void SetCollisionLayer(int layerIndex) { collisionLayer = layerIndex; }

            static void ApplySnapshot(Component* target, const ComponentSnapshot& snap);

            static void SnapshotComponent(ComponentSnapshot& snap, const Component* comp);
        private:
            std::string name;
            std::string sourceUuid;
            Math::Vector3 position;
            Math::Quaternion rotation;
            Math::Vector3 scale = Math::Vector3(1.0f, 1.0f, 1.0f);
            int collisionLayer = 0;
            std::vector<ComponentSnapshot> componentSnapshots;
            std::vector<std::unique_ptr<Prefab>> childPrefabs;
        };

    }
}
