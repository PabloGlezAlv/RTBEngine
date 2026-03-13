#include "Prefab.h"
#include <cstring>

#include "GameObject.h"
#include "Component.h"
#include "../Reflection/TypeInfo.h"
#include "../Scripting/ComponentRegistry.h"
#include "../ECS/SceneManager.h"

namespace RTBEngine {
    namespace ECS {

        Prefab::Prefab(const std::string& name)
            : name(name)
        {
        }

        Prefab::~Prefab()
        {
        }

        void Prefab::SnapshotComponent(ComponentSnapshot& snap, const Component* comp)
        {
            snap.typeName = comp->GetTypeName();

            const Reflection::TypeInfo* typeInfo = comp->GetTypeInfo();
            if (!typeInfo) return;

            for (const Reflection::PropertyInfo* prop : typeInfo->GetSerializableProperties())
            {
                // Write: [offset 8 bytes][size 8 bytes][data N bytes]
                size_t offset = prop->offset;
                size_t size = prop->size;

                const char* src = reinterpret_cast<const char*>(comp) + offset;

                // Append offset
                snap.rawData.insert(snap.rawData.end(),
                    reinterpret_cast<uint8_t*>(&offset),
                    reinterpret_cast<uint8_t*>(&offset) + sizeof(size_t));

                // Append size
                snap.rawData.insert(snap.rawData.end(),
                    reinterpret_cast<uint8_t*>(&size),
                    reinterpret_cast<uint8_t*>(&size) + sizeof(size_t));

                // Append data
                snap.rawData.insert(snap.rawData.end(),
                    reinterpret_cast<const uint8_t*>(src),
                    reinterpret_cast<const uint8_t*>(src) + size);
            }
        }

        void Prefab::ApplySnapshot(Component* target, const ComponentSnapshot& snap)
        {
            const uint8_t* ptr = snap.rawData.data();
            const uint8_t* end = ptr + snap.rawData.size();

            while (ptr < end)
            {
                // Read offset
                size_t offset = *reinterpret_cast<const size_t*>(ptr);
                ptr += sizeof(size_t);

                // Read size
                size_t size = *reinterpret_cast<const size_t*>(ptr);
                ptr += sizeof(size_t);

                // Write data to target
                char* dst = reinterpret_cast<char*>(target) + offset;
                std::memcpy(dst, ptr, size);
                ptr += size;
            }
        }

        std::unique_ptr<Prefab> Prefab::CreateFromGameObject(const GameObject* source)
        {
            auto prefab = std::make_unique<Prefab>(source->GetName());

            for (const auto& comp : source->GetComponents())
            {
                ComponentSnapshot snap;
                SnapshotComponent(snap, comp.get());
                prefab->componentSnapshots.push_back(std::move(snap));
            }

            return prefab;
        }

        GameObject* Prefab::Instantiate(GameObject* parent) const
        {
            auto* go = new GameObject(name);

            for (const ComponentSnapshot& snap : componentSnapshots)
            {
                Component* comp = Scripting::ComponentRegistry::GetInstance().CreateComponent(snap.typeName);
                if (!comp) continue;

                ApplySnapshot(comp, snap);
                go->AddComponent(comp);
            }

            if (parent)
                go->SetParent(parent);

            SceneManager::GetInstance().GetActiveScene()->AddGameObject(go);

            return go;
        }

    }
}
