#include "Prefab.h"
#include <cstring>

#include "GameObject.h"
#include "Component.h"
#include "MeshRenderer.h"
#include "SceneManager.h"
#include "../Reflection/ListPropertyAccess.h"
#include "../Scripting/ComponentRegistry.h"
#include "../Scripting/SceneReflectionUtils.h"
#include "../Core/ResourceManager.h"
#include "../Core/Logger.h"
#include "../Rendering/Mesh.h"
#include "../Rendering/Texture.h"
#include "../Rendering/FbxBinding.h"
#include "../Rendering/ModelLoader.h"
#include "../Animation/Animator.h"
#include "../Audio/AudioClip.h"

#include <unordered_map>

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
            if (!comp) {
                return;
            }

            const char* typeName = comp->GetTypeName();
            if (!typeName || typeName[0] == '\0') {
                return;
            }

            snap.typeName = typeName;

            // Use TypeRegistry by name — safe across RTBEngine / GameScripts DLL boundaries.
            const Reflection::TypeInfo* typeInfo =
                Reflection::TypeRegistry::GetInstance().GetTypeInfo(snap.typeName);
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

                if (prop->type == Reflection::PropertyType::GameObjectRef)
                {
                    const GameObject* target = *static_cast<GameObject* const*>(prop->GetData(comp));
                    if (target)
                        snap.ptrPathData[offset] = target->GetUUID();
                    continue;
                }

                if (prop->type == Reflection::PropertyType::ComponentRef)
                {
                    const Component* target = *static_cast<Component* const*>(prop->GetData(comp));
                    if (target && target->GetOwner()) {
                        snap.ptrPathData[offset] =
                            target->GetOwner()->GetUUID() + "/" + std::string(target->GetTypeName());
                    }
                    continue;
                }

                if (prop->type == Reflection::PropertyType::List) {
                    switch (prop->listElementType) {
                    case Reflection::ListElementType::String:
                    case Reflection::ListElementType::AssetRef: {
                        const auto* values = static_cast<const std::vector<std::string>*>(
                            prop->GetData(comp));
                        if (values) {
                            snap.listStringData[offset] = *values;
                        }
                        continue;
                    }
                    case Reflection::ListElementType::GameObjectRef:
                    case Reflection::ListElementType::ComponentRef:
                        // Scene references are resolved when the prefab instance is loaded.
                        continue;
                    case Reflection::ListElementType::AnimationKeyClip: {
                        const auto* values = static_cast<const std::vector<RTBEngine::Animation::AnimationKeyClip>*>(
                            prop->GetData(comp));
                        if (values) {
                            std::vector<AnimationKeyClipSnapshot> storedValues;
                            storedValues.reserve(values->size());
                            for (const RTBEngine::Animation::AnimationKeyClip& entry : *values) {
                                AnimationKeyClipSnapshot stored;
                                stored.key = entry.key;
                                stored.clipFbxRef = entry.clipFbxRef;
                                stored.loop = entry.loop;
                                storedValues.push_back(std::move(stored));
                            }
                            snap.listAnimationKeyClipData[offset] = std::move(storedValues);
                        }
                        continue;
                    }
                    default:
                        continue;
                    }
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
            if (!actualObject) {
                return;
            }

            const uint8_t* ptr = snap.rawData.data();
            const uint8_t* end = ptr + snap.rawData.size();
            const Reflection::TypeInfo* typeInfo =
                Reflection::TypeRegistry::GetInstance().GetTypeInfo(snap.typeName);

            while (ptr < end)
            {
                size_t offset = *reinterpret_cast<const size_t*>(ptr);
                ptr += sizeof(size_t);

                size_t size = *reinterpret_cast<const size_t*>(ptr);
                ptr += sizeof(size_t);

                bool skipMemcpy = false;
                if (typeInfo) {
                    for (const Reflection::PropertyInfo* prop : typeInfo->GetSerializableProperties()) {
                        if (prop && prop->offset == offset && prop->size == size &&
                            prop->type == Reflection::PropertyType::List) {
                            skipMemcpy = true;
                            break;
                        }
                    }
                }

                if (!skipMemcpy) {
                    std::memcpy(actualObject + offset, ptr, size);
                }
                ptr += size;
            }

            for (const auto& [offset, str] : snap.stringData)
            {
                std::string* dst = reinterpret_cast<std::string*>(actualObject + offset);
                *dst = str;
            }

            for (const auto& [offset, strings] : snap.listStringData)
            {
                std::vector<std::string>* dst = reinterpret_cast<std::vector<std::string>*>(
                    actualObject + offset);
                *dst = strings;
            }

            for (const auto& [offset, storedValues] : snap.listAnimationKeyClipData)
            {
                auto* dst = reinterpret_cast<std::vector<RTBEngine::Animation::AnimationKeyClip>*>(
                    actualObject + offset);
                if (!dst) {
                    continue;
                }

                dst->clear();
                dst->reserve(storedValues.size());
                for (const AnimationKeyClipSnapshot& stored : storedValues) {
                    RTBEngine::Animation::AnimationKeyClip entry;
                    entry.key = stored.key;
                    entry.clipFbxRef = stored.clipFbxRef;
                    entry.loop = stored.loop;
                    dst->push_back(std::move(entry));
                }
            }

            if (snap.ptrPathData.empty()) return;

            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();
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
                        const Rendering::ModelData& modelData = resources.LoadModelData(path);

                        if (!modelData.meshes.empty())
                        {
                            auto* meshRenderer = dynamic_cast<MeshRenderer*>(target);

                            Rendering::FbxBindingContext ctx{ resources, path, modelData };
                            Rendering::FbxBindingResult bind = Rendering::BuildMeshesAndMaterials(ctx);

                            std::string shaderName = "basic";
                            if (meshRenderer && !meshRenderer->shaderRef.empty()) {
                                shaderName = meshRenderer->shaderRef;
                            }
                            Rendering::Shader* shader = resources.ResolveShader(shaderName);

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
                    Rendering::Texture* tex = nullptr;
                    if (!path.empty()) {
                        if (path.size() > 8 && path.substr(path.size() - 8) == ".texture") {
                            tex = resources.LoadTextureAsset(path);
                        }
                        else if (dynamic_cast<MeshRenderer*>(target)) {
                            tex = resources.LoadModelTexture(path);
                        }
                        else {
                            tex = resources.LoadTexture(path);
                        }
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
            prefab->sourceUuid = source->GetUUID();

            const auto& transform = source->GetTransform();
            prefab->position = transform.GetPosition();
            prefab->rotation = transform.GetRotation();
            prefab->scale = transform.GetScale();
            prefab->collisionLayer = source->GetCollisionLayer();

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

        GameObject* Prefab::Instantiate(GameObject* parent, std::vector<GameObject*>& outChildren, bool regenerateUuids) const
        {
            struct PendingReferencePatch {
                Component* component = nullptr;
                const Reflection::PropertyInfo* property = nullptr;
                std::string referenceValue;
            };

            struct InstantiateContext {
                std::unordered_map<std::string, GameObject*> sourceUuidToInstance;
                std::unordered_map<std::string, std::string> uuidRemap;
                std::vector<PendingReferencePatch> pendingReferencePatches;
                std::vector<Component*> createdComponents;
                Scene* activeScene = nullptr;
            };

            InstantiateContext context;
            context.activeScene = SceneManager::GetInstance().GetActiveScene();

            auto resolveReferenceUuid = [&](const std::string& referenceUuid) -> std::string {
                auto remapIt = context.uuidRemap.find(referenceUuid);
                if (remapIt != context.uuidRemap.end()) {
                    return remapIt->second;
                }
                return referenceUuid;
            };

            std::function<GameObject*(const Prefab&, GameObject*)> instantiateNode =
                [&](const Prefab& nodePrefab, GameObject* nodeParent) -> GameObject*
            {
                auto* go = new GameObject(nodePrefab.name);
                go->SetPrefabName(nodePrefab.name);
                go->GetTransform().SetPosition(nodePrefab.position);
                go->GetTransform().SetRotation(nodePrefab.rotation);
                go->GetTransform().SetScale(nodePrefab.scale);
                go->SetCollisionLayer(nodePrefab.collisionLayer);

                std::string instanceUuid = nodePrefab.sourceUuid;
                if (regenerateUuids || instanceUuid.empty()) {
                    instanceUuid = GameObject::GenerateNewUUID();
                }

                if (!instanceUuid.empty()) {
                    if (regenerateUuids && !nodePrefab.sourceUuid.empty()) {
                        context.uuidRemap[nodePrefab.sourceUuid] = instanceUuid;
                    }
                    go->SetUUID(instanceUuid);
                    context.sourceUuidToInstance[instanceUuid] = go;
                }

                for (const ComponentSnapshot& snap : nodePrefab.componentSnapshots)
                {
                    Component* comp = Scripting::ComponentRegistry::GetInstance().CreateComponent(snap.typeName);
                    if (!comp) continue;

                    const Reflection::TypeInfo* registeredTypeInfo =
                        Scripting::ComponentRegistry::GetInstance().GetComponentTypeInfo(snap.typeName);
                    const Reflection::TypeInfo* effectiveTypeInfo = registeredTypeInfo;

                    ApplySnapshot(comp, snap);

                    Scripting::SceneReflectionUtils::ClearReferenceProperties(comp, registeredTypeInfo);
                    go->AddComponent(comp, registeredTypeInfo);
                    context.createdComponents.push_back(comp);

                    if (!effectiveTypeInfo) {
                        continue;
                    }

                    for (const Reflection::PropertyInfo* prop : effectiveTypeInfo->GetSerializableProperties()) {
                        if (!prop ||
                            (prop->type != Reflection::PropertyType::GameObjectRef &&
                             prop->type != Reflection::PropertyType::ComponentRef)) {
                            continue;
                        }

                        auto patchIt = snap.ptrPathData.find(prop->offset);
                        if (patchIt == snap.ptrPathData.end() || patchIt->second.empty()) {
                            continue;
                        }

                        context.pendingReferencePatches.push_back({ comp, prop, patchIt->second });
                    }
                }

                if (nodeParent) {
                    go->SetParent(nodeParent);
                }

                for (const auto& childPrefab : nodePrefab.childPrefabs)
                {
                    if (!childPrefab) continue;
                    instantiateNode(*childPrefab, go);
                }

                return go;
            };

            GameObject* root = instantiateNode(*this, parent);

            // Register every descendant with the scene, not only direct children.
            // Nested nodes (e.g. weapons parented to bones) must be in the flat GO list
            // so component caches, lifecycle, and rendering can find them.
            std::function<void(GameObject*)> collectDescendants = [&](GameObject* node) {
                if (!node) {
                    return;
                }

                for (GameObject* child : node->GetChildren()) {
                    outChildren.push_back(child);
                    collectDescendants(child);
                }
            };
            collectDescendants(root);

            for (const PendingReferencePatch& patch : context.pendingReferencePatches)
            {
                if (!patch.component || !patch.property) {
                    continue;
                }

                if (patch.property->type == Reflection::PropertyType::GameObjectRef) {
                    GameObject* resolvedObject = nullptr;
                    const std::string resolvedUuid = resolveReferenceUuid(patch.referenceValue);

                    auto localIt = context.sourceUuidToInstance.find(resolvedUuid);
                    if (localIt != context.sourceUuidToInstance.end()) {
                        resolvedObject = localIt->second;
                    } else if (context.activeScene) {
                        resolvedObject = context.activeScene->FindGameObjectByUUID(resolvedUuid);
                    }

                    void* data = patch.property->GetMutableData(patch.component);
                    if (data) {
                        *static_cast<GameObject**>(data) = resolvedObject;
                    }
                    continue;
                }

                if (patch.property->type == Reflection::PropertyType::ComponentRef) {
                    const size_t slash = patch.referenceValue.find('/');
                    if (slash == std::string::npos) {
                        continue;
                    }

                    const std::string targetUuid = resolveReferenceUuid(patch.referenceValue.substr(0, slash));
                    const std::string targetType = patch.referenceValue.substr(slash + 1);

                    GameObject* targetGameObject = nullptr;
                    auto localIt = context.sourceUuidToInstance.find(targetUuid);
                    if (localIt != context.sourceUuidToInstance.end()) {
                        targetGameObject = localIt->second;
                    } else if (context.activeScene) {
                        targetGameObject = context.activeScene->FindGameObjectByUUID(targetUuid);
                    }

                    Component* resolvedComponent = nullptr;
                    if (targetGameObject) {
                        for (const auto& component : targetGameObject->GetComponents()) {
                            if (component && std::string(component->GetTypeName()) == targetType) {
                                resolvedComponent = component.get();
                                break;
                            }
                        }
                    }

                    void* data = patch.property->GetMutableData(patch.component);
                    if (data) {
                        *static_cast<Component**>(data) = resolvedComponent;
                    }
                }
            }

            return root;
        }

        GameObject* Prefab::Instantiate(GameObject* parent, bool regenerateUuids) const
        {
            std::vector<GameObject*> discarded;
            return Instantiate(parent, discarded, regenerateUuids);
        }

    }
}
