#include "Prefab.h"
#include <cstring>

#include "GameObject.h"
#include "Component.h"
#include "MeshRenderer.h"
#include "../Reflection/TypeInfo.h"
#include "../Scripting/ComponentRegistry.h"
#include "../Core/ResourceManager.h"
#include "../Core/Logger.h"
#include "../Rendering/Mesh.h"
#include "../Rendering/Texture.h"
#include "../Rendering/FbxBinding.h"
#include "../Rendering/ModelLoader.h"
#include "../Audio/AudioClip.h"

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

            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();

            for (const Reflection::PropertyInfo* prop : typeInfo->GetSerializableProperties())
            {
                size_t offset = prop->offset;
                size_t size = prop->size;

                if (prop->type == Reflection::PropertyType::String ||
                    prop->type == Reflection::PropertyType::AssetRef)
                {
                    const std::string* strPtr = static_cast<const std::string*>(prop->GetData(comp));
                    snap.stringData[offset] = *strPtr;
                    continue;
                }

                if (prop->type == Reflection::PropertyType::MeshRef)
                {
                    const Rendering::Mesh* mesh = *static_cast<Rendering::Mesh* const*>(prop->GetData(comp));
                    if (mesh) {
                        std::string meshPath = resources.GetMeshPath(const_cast<Rendering::Mesh*>(mesh));
                        snap.ptrPathData[offset] = meshPath;
                    }
                    continue;
                }

                if (prop->type == Reflection::PropertyType::TextureRef)
                {
                    const Rendering::Texture* tex = *static_cast<Rendering::Texture* const*>(prop->GetData(comp));
                    if (tex)
                        snap.ptrPathData[offset] = resources.GetTexturePath(const_cast<Rendering::Texture*>(tex));
                    continue;
                }

                if (prop->type == Reflection::PropertyType::AudioClipRef)
                {
                    const Audio::AudioClip* clip = *static_cast<Audio::AudioClip* const*>(prop->GetData(comp));
                    if (clip)
                        snap.ptrPathData[offset] = resources.GetAudioClipPath(const_cast<Audio::AudioClip*>(clip));
                    continue;
                }

                const char* src = static_cast<const char*>(prop->GetData(comp));

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
            char* actualObject = static_cast<char*>(target ? target->GetActualObject() : nullptr);
            const uint8_t* ptr = snap.rawData.data();
            const uint8_t* end = ptr + snap.rawData.size();

            while (ptr < end)
            {
                size_t offset = *reinterpret_cast<const size_t*>(ptr);
                ptr += sizeof(size_t);

                size_t size = *reinterpret_cast<const size_t*>(ptr);
                ptr += sizeof(size_t);

                char* dst = actualObject + offset;
                std::memcpy(dst, ptr, size);
                ptr += size;
            }

            for (const auto& [offset, str] : snap.stringData)
            {
                std::string* dst = reinterpret_cast<std::string*>(actualObject + offset);
                *dst = str;
            }

            if (snap.ptrPathData.empty()) return;

            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();
            const Reflection::TypeInfo* typeInfo = target->GetTypeInfo();
            if (!typeInfo) return;

            for (const Reflection::PropertyInfo* prop : typeInfo->GetSerializableProperties())
            {
                auto it = snap.ptrPathData.find(prop->offset);
                if (it == snap.ptrPathData.end()) continue;

                const std::string& path = it->second;
                char* dst = static_cast<char*>(prop->GetMutableData(target));

                if (prop->type == Reflection::PropertyType::MeshRef)
                {
                    Rendering::Mesh* mesh = nullptr;
                    if (!path.empty())
                    {
                        Rendering::ModelData modelData = Rendering::ModelLoader::LoadModelWithAnimations(path);

                        if (!modelData.meshes.empty())
                        {
                            resources.RegisterMeshes(path, modelData.meshes);

                            auto* meshRenderer = dynamic_cast<MeshRenderer*>(target);

                            Rendering::FbxBindingContext ctx{ resources, path, modelData };
                            Rendering::FbxBindingResult bind = Rendering::BuildMeshesAndMaterials(ctx);
                            Rendering::Shader* shader = resources.GetShader("basic");

                            // Pick mesh by meshIndex
                            const Reflection::PropertyInfo* indexProp = typeInfo->GetProperty("meshIndex");
                            int idx = 0;
                            if (indexProp)
                            {
                                const char* idxSrc = static_cast<const char*>(indexProp->GetData(target));
                                std::memcpy(&idx, idxSrc, sizeof(int));
                            }
                            int clampedIdx = (idx >= 0 && idx < static_cast<int>(modelData.meshes.size())) ? idx : 0;
                            mesh = modelData.meshes[clampedIdx];

                            if (meshRenderer)
                            {
                                Rendering::Material* mat = nullptr;
                                if (clampedIdx < static_cast<int>(bind.meshMaterials.size()))
                                    mat = bind.meshMaterials[clampedIdx];

                                if (mat)
                                {
                                    if (shader) mat->SetShader(shader);
                                    meshRenderer->SetMaterial(mat);
                                }
                            }
                        }
                    }
                    std::memcpy(dst, &mesh, sizeof(Rendering::Mesh*));
                }
                else if (prop->type == Reflection::PropertyType::TextureRef)
                {
                    // .texture assets carry flip metadata; raw images use default flip
                    Rendering::Texture* tex = nullptr;
                    if (!path.empty()) {
                        tex = (path.size() > 8 && path.substr(path.size() - 8) == ".texture")
                            ? resources.LoadTextureAsset(path)
                            : resources.LoadTexture(path);
                    }
                    std::memcpy(dst, &tex, sizeof(Rendering::Texture*));
                }
                else if (prop->type == Reflection::PropertyType::AudioClipRef)
                {
                    Audio::AudioClip* clip = path.empty() ? nullptr : resources.LoadAudioClip(path);
                    std::memcpy(dst, &clip, sizeof(Audio::AudioClip*));
                }
            }
        }


        std::unique_ptr<Prefab> Prefab::CreateFromGameObject(const GameObject* source)
        {
            auto prefab = std::make_unique<Prefab>(source->GetName());

            const auto& transform = source->GetTransform();
            prefab->position = transform.GetPosition();
            prefab->rotation = transform.GetRotation();
            prefab->scale = transform.GetScale();

            for (const auto& comp : source->GetComponents())
            {
                ComponentSnapshot snap;
                SnapshotComponent(snap, comp.get());
                prefab->componentSnapshots.push_back(std::move(snap));
            }

            // Recursively snapshot all child GameObjects (skip transient bone GOs)
            for (const auto* child : source->GetChildren())
            {
                if (child && !child->IsTransient())
                    prefab->childPrefabs.push_back(CreateFromGameObject(child));
            }

            return prefab;
        }

        GameObject* Prefab::Instantiate(GameObject* parent, std::vector<GameObject*>& outChildren) const
        {
            auto* go = new GameObject(name);
            go->SetPrefabName(name);

            go->GetTransform().SetPosition(position);
            go->GetTransform().SetRotation(rotation);
            go->GetTransform().SetScale(scale);

            for (const ComponentSnapshot& snap : componentSnapshots)
            {
                Component* comp = Scripting::ComponentRegistry::GetInstance().CreateComponent(snap.typeName);
                if (!comp) continue;

                const Reflection::TypeInfo* registeredTypeInfo =
                    Scripting::ComponentRegistry::GetInstance().GetComponentTypeInfo(snap.typeName);

                ApplySnapshot(comp, snap);
                go->AddComponent(comp, registeredTypeInfo);
                comp->OnValidate();
            }

            if (parent)
                go->SetParent(parent);

            // Recursively instantiate children; collect them all in outChildren so the
            // caller can add them to the scene (Scene::Render only iterates the flat list).
            for (const auto& childPrefab : childPrefabs)
            {
                if (!childPrefab) continue;
                GameObject* child = childPrefab->Instantiate(go, outChildren);
                if (child)
                    outChildren.push_back(child);
            }

            return go;
        }

        GameObject* Prefab::Instantiate(GameObject* parent) const
        {
            std::vector<GameObject*> discarded;
            return Instantiate(parent, discarded);
        }

    }
}
