#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <unordered_map>


namespace RTBEngine {
    namespace ECS {

        class GameObject;
        class Component;

        struct ComponentSnapshot {
            std::string typeName;
            std::vector<uint8_t> rawData;
            std::unordered_map<size_t, std::string> stringData;
        };

        class Prefab {
        public:
            Prefab(const std::string& name = "Prefab");
            ~Prefab();

            Prefab(const Prefab&) = delete;
            Prefab& operator=(const Prefab&) = delete;

            static std::unique_ptr<Prefab> CreateFromGameObject(const GameObject* source);
            GameObject* Instantiate(GameObject* parent = nullptr) const;

            const std::string& GetName() const { return name; }
            const std::vector<ComponentSnapshot>& GetSnapshots() const { return componentSnapshots; }

            static void ApplySnapshot(Component* target, const ComponentSnapshot& snap);

        private:
            std::string name;
            std::vector<ComponentSnapshot> componentSnapshots;

            static void SnapshotComponent(ComponentSnapshot& snap, const Component* comp);
        };

    }
}
