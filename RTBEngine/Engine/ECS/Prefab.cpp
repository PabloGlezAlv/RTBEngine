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
                size_t offset = prop->offset;
                size_t size = prop->size;

                if (prop->type == Reflection::PropertyType::String)
                {
                    const std::string* strPtr = reinterpret_cast<const std::string*>(
                        reinterpret_cast<const char*>(comp) + offset);
                    snap.stringData[offset] = *strPtr;
                    continue;
                }

                const char* src = reinterpret_cast<const char*>(comp) + offset;

                snap.rawData.insert(snap.rawData.end(),
                    reinterpret_cast<const uint8_t*>(&offset),
                    reinterpret_cast<const uint8_t*>(&offset) + sizeof(size_t));

                snap.rawData.insert(snap.rawData.end(),
                    reinterpret_cast<const uint8_t*>(&size),
                    reinterpret_cast<const uint8_t*>(&size) + sizeof(size_t));

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
                size_t offset = *reinterpret_cast<const size_t*>(ptr);
                ptr += sizeof(size_t);

                size_t size = *reinterpret_cast<const size_t*>(ptr);
                ptr += sizeof(size_t);

                char* dst = reinterpret_cast<char*>(target) + offset;
                std::memcpy(dst, ptr, size);
                ptr += size;
            }

            for (const auto& [offset, str] : snap.stringData)
            {
                std::string* dst = reinterpret_cast<std::string*>(
                    reinterpret_cast<char*>(target) + offset);
                *dst = str;
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
