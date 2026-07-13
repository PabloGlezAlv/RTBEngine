#pragma once
#include "../RTBEngineAPI.h"
#include "../Math/Math.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <unordered_map>


namespace RTBEngine {
    namespace Reflection {
        class TypeInfo;
        struct PropertyInfo;
    }

    namespace ECS {

        class GameObject;
        class Component;
        class Scene;

        struct RTB_API AnimationKeyClipSnapshot {
            std::string key;
            std::string clipFbxRef;
            bool loop = false;
        };

        struct RTB_API ComponentSnapshot {
            std::string typeName;
            std::vector<uint8_t> rawData;
            std::unordered_map<size_t, std::string> stringData;
            std::unordered_map<size_t, std::string> ptrPathData;
            std::unordered_map<size_t, std::vector<std::string>> listStringData;
            std::unordered_map<size_t, std::vector<AnimationKeyClipSnapshot>> listAnimationKeyClipData;
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
            GameObject* Instantiate(GameObject* parent, std::vector<GameObject*>& outChildren, bool regenerateUuids = false) const;
            // Convenience overload for flat prefabs (no children).
            GameObject* Instantiate(GameObject* parent = nullptr, bool regenerateUuids = false) const;

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

            void SetSourceUuid(const std::string& uuid) { sourceUuid = uuid; }
            const std::string& GetSourceUuid() const { return sourceUuid; }

            static void ApplySnapshot(Component* target, const ComponentSnapshot& snap);
            static void ApplySnapshotProperty(
                Component* target,
                const ComponentSnapshot& snap,
                const Reflection::PropertyInfo* property,
                Scene* referenceScene,
                GameObject* referenceRoot);

            static void SnapshotComponent(ComponentSnapshot& snap, const Component* comp);
            static void SnapshotProperty(
                ComponentSnapshot& snap,
                const Component* comp,
                const Reflection::PropertyInfo* property);

            std::unique_ptr<Prefab> DeepClone() const;
            Prefab* FindMutableChildByPath(const std::vector<std::string>& nodePath);
            const Prefab* FindChildByPath(const std::vector<std::string>& nodePath) const;
            static void CopySourceUuidsFrom(const Prefab& source, Prefab& destination);

            std::vector<ComponentSnapshot>& GetMutableSnapshots() { return componentSnapshots; }
            std::vector<std::unique_ptr<Prefab>>& GetMutableChildPrefabs() { return childPrefabs; }
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
