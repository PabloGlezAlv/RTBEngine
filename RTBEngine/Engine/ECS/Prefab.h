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
        };

        class RTB_API Prefab {
        public:
            Prefab(const std::string& name = "Prefab");
            ~Prefab();

            Prefab(const Prefab&) = delete;
            Prefab& operator=(const Prefab&) = delete;

            void AddSnapshot(ComponentSnapshot&& snap) { componentSnapshots.push_back(std::move(snap)); }

            static std::unique_ptr<Prefab> CreateFromGameObject(const GameObject* source);
            GameObject* Instantiate(GameObject* parent = nullptr) const;

            const std::string& GetName() const { return name; }
            const std::vector<ComponentSnapshot>& GetSnapshots() const { return componentSnapshots; }

            static void ApplySnapshot(Component* target, const ComponentSnapshot& snap);

            static void SnapshotComponent(ComponentSnapshot& snap, const Component* comp);
        private:
            std::string name;
            std::vector<ComponentSnapshot> componentSnapshots;
        };

    }
}
