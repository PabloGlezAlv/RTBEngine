#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace RTBEngine {
    namespace ECS {

        class GameObject;
        class Component;

        struct ComponentSnapshot {
            std::string typeName;
            std::vector<uint8_t> rawData;
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

        private:
            std::string name;
            std::vector<ComponentSnapshot> componentSnapshots;

            static void SnapshotComponent(ComponentSnapshot& snap, const Component* comp);
            static void ApplySnapshot(Component* target, const ComponentSnapshot& snap);
        };

    }
}
