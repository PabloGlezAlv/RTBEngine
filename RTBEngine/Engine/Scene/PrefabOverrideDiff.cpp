#include "PrefabOverrideDiff.h"
#include "GameObject.h"
#include "Component.h"
#include "../Animation/Animator.h"
#include "../Audio/AudioClip.h"
#include "../Core/ResourceManager.h"
#include "../Rendering/Mesh.h"
#include "../Rendering/Texture.h"
#include <cstring>

namespace RTBEngine {
    namespace Scene {

        namespace {
            const uint8_t* FindRawBytes(const ComponentSnapshot& snap, size_t offset, size_t& outSize)
            {
                const uint8_t* raw = snap.rawData.data();
                const uint8_t* rawEnd = raw + snap.rawData.size();

                while (raw < rawEnd) {
                    size_t rawOffset = *reinterpret_cast<const size_t*>(raw);
                    raw += sizeof(size_t);
                    size_t rawSize = *reinterpret_cast<const size_t*>(raw);
                    raw += sizeof(size_t);

                    if (rawOffset == offset) {
                        outSize = rawSize;
                        return raw;
                    }
                    raw += rawSize;
                }

                outSize = 0;
                return nullptr;
            }

            std::string GetLivePtrPath(
                const Component* component,
                const Reflection::PropertyInfo* property)
            {
                Core::ResourceManager& resources = Core::ResourceManager::GetInstance();

                if (property->type == Reflection::PropertyType::MeshRef) {
                    const Rendering::Mesh* mesh = *static_cast<Rendering::Mesh* const*>(property->GetData(component));
                    return mesh ? resources.GetMeshPath(const_cast<Rendering::Mesh*>(mesh)) : std::string();
                }

                if (property->type == Reflection::PropertyType::TextureRef) {
                    const Rendering::Texture* tex = *static_cast<Rendering::Texture* const*>(property->GetData(component));
                    return tex ? resources.GetTexturePath(const_cast<Rendering::Texture*>(tex)) : std::string();
                }

                if (property->type == Reflection::PropertyType::AudioClipRef) {
                    const Audio::AudioClip* clip = *static_cast<Audio::AudioClip* const*>(property->GetData(component));
                    return clip ? resources.GetAudioClipPath(const_cast<Audio::AudioClip*>(clip)) : std::string();
                }

                if (property->type == Reflection::PropertyType::GameObjectRef) {
                    const GameObject* target = *static_cast<GameObject* const*>(property->GetData(component));
                    return target ? target->GetUUID() : std::string();
                }

                if (property->type == Reflection::PropertyType::ComponentRef) {
                    const Component* target = *static_cast<Component* const*>(property->GetData(component));
                    if (target && target->GetOwner()) {
                        return target->GetOwner()->GetUUID() + "/" + std::string(target->GetTypeName());
                    }
                    return std::string();
                }

                return std::string();
            }
        }

        const ComponentSnapshot* PrefabOverrideDiff::FindBaselineSnapshot(
            const Prefab* baselineNode,
            const char* typeName)
        {
            if (!baselineNode || !typeName || typeName[0] == '\0') {
                return nullptr;
            }

            for (const ComponentSnapshot& snap : baselineNode->GetSnapshots()) {
                if (snap.typeName == typeName) {
                    return &snap;
                }
            }

            return nullptr;
        }

        bool PrefabOverrideDiff::IsPropertyOverridden(
            const Component* component,
            const ComponentSnapshot* baselineSnapshot,
            const Reflection::PropertyInfo* property)
        {
            if (!component || !property) {
                return false;
            }

            if (!baselineSnapshot) {
                return true;
            }

            const size_t offset = property->offset;
            const size_t size = property->size;

            if (property->type == Reflection::PropertyType::String ||
                property->type == Reflection::PropertyType::AssetRef) {
                const std::string* liveStr = static_cast<const std::string*>(property->GetData(component));
                auto it = baselineSnapshot->stringData.find(offset);
                return it == baselineSnapshot->stringData.end() || it->second != *liveStr;
            }

            if (property->type == Reflection::PropertyType::MeshRef ||
                property->type == Reflection::PropertyType::TextureRef ||
                property->type == Reflection::PropertyType::AudioClipRef ||
                property->type == Reflection::PropertyType::GameObjectRef ||
                property->type == Reflection::PropertyType::ComponentRef) {
                const std::string livePath = GetLivePtrPath(component, property);
                auto it = baselineSnapshot->ptrPathData.find(offset);
                const std::string baselinePath = it != baselineSnapshot->ptrPathData.end() ? it->second : std::string();
                return livePath != baselinePath;
            }

            if (property->type == Reflection::PropertyType::List) {
                switch (property->listElementType) {
                case Reflection::ListElementType::String:
                case Reflection::ListElementType::AssetRef: {
                    const auto* liveValues = static_cast<const std::vector<std::string>*>(
                        property->GetData(component));
                    auto it = baselineSnapshot->listStringData.find(offset);
                    if (it == baselineSnapshot->listStringData.end()) {
                        return liveValues && !liveValues->empty();
                    }
                    if (!liveValues) {
                        return !it->second.empty();
                    }
                    return *liveValues != it->second;
                }
                case Reflection::ListElementType::AnimationKeyClip: {
                    const auto* liveValues = static_cast<const std::vector<RTBEngine::Animation::AnimationKeyClip>*>(
                        property->GetData(component));
                    auto it = baselineSnapshot->listAnimationKeyClipData.find(offset);
                    if (!liveValues) {
                        return it != baselineSnapshot->listAnimationKeyClipData.end() && !it->second.empty();
                    }

                    std::vector<AnimationKeyClipSnapshot> liveSnap;
                    liveSnap.reserve(liveValues->size());
                    for (const RTBEngine::Animation::AnimationKeyClip& entry : *liveValues) {
                        AnimationKeyClipSnapshot stored;
                        stored.key = entry.key;
                        stored.clipFbxRef = entry.clipFbxRef;
                        stored.loop = entry.loop;
                        liveSnap.push_back(std::move(stored));
                    }

                    if (it == baselineSnapshot->listAnimationKeyClipData.end()) {
                        return !liveSnap.empty();
                    }

                    if (liveSnap.size() != it->second.size()) {
                        return true;
                    }

                    for (size_t index = 0; index < liveSnap.size(); ++index) {
                        const AnimationKeyClipSnapshot& liveEntry = liveSnap[index];
                        const AnimationKeyClipSnapshot& baselineEntry = it->second[index];
                        if (liveEntry.key != baselineEntry.key ||
                            liveEntry.clipFbxRef != baselineEntry.clipFbxRef ||
                            liveEntry.loop != baselineEntry.loop) {
                            return true;
                        }
                    }
                    return false;
                }
                default:
                    break;
                }
            }

            size_t rawSize = 0;
            const uint8_t* snapBytes = FindRawBytes(*baselineSnapshot, offset, rawSize);
            const char* liveBytes = static_cast<const char*>(property->GetData(component));

            if (!snapBytes || rawSize != size) {
                return true;
            }

            return std::memcmp(liveBytes, snapBytes, size) != 0;
        }

        std::vector<const Reflection::PropertyInfo*> PrefabOverrideDiff::GetOverriddenProperties(
            const Component* component,
            const ComponentSnapshot* baselineSnapshot)
        {
            std::vector<const Reflection::PropertyInfo*> result;
            if (!component) {
                return result;
            }

            const Reflection::TypeInfo* typeInfo = component->GetTypeInfo();
            if (!typeInfo) {
                return result;
            }

            for (const Reflection::PropertyInfo* prop : typeInfo->GetSerializableProperties()) {
                if (IsPropertyOverridden(component, baselineSnapshot, prop)) {
                    result.push_back(prop);
                }
            }

            return result;
        }

        bool PrefabOverrideDiff::IsAddedComponent(
            const Component* component,
            const Prefab* baselineNode)
        {
            if (!component || !baselineNode) {
                return false;
            }

            return FindBaselineSnapshot(baselineNode, component->GetTypeName()) == nullptr;
        }

        bool PrefabOverrideDiff::IsTransformOverridden(
            const GameObject* gameObject,
            const Prefab* baselineNode)
        {
            if (!gameObject || !baselineNode || gameObject->IsAnimatorBone()) {
                return false;
            }

            const auto& transform = gameObject->GetTransform();
            return transform.GetPosition() != baselineNode->GetPosition()
                || transform.GetRotation() != baselineNode->GetRotation()
                || transform.GetScale() != baselineNode->GetScale();
        }

        bool PrefabOverrideDiff::IsActiveOverridden(const GameObject* gameObject)
        {
            return gameObject && !gameObject->IsActive();
        }

        bool PrefabOverrideDiff::IsCollisionLayerOverridden(
            const GameObject* gameObject,
            const Prefab* baselineNode)
        {
            if (!gameObject || !baselineNode) {
                return false;
            }

            return gameObject->GetCollisionLayer() != baselineNode->GetCollisionLayer();
        }

        bool PrefabOverrideDiff::IsSceneOnlyChild(
            const GameObject* child,
            const Prefab* parentBaseline)
        {
            if (!child || !parentBaseline || child->IsTransient()) {
                return false;
            }

            for (const auto& baselineChild : parentBaseline->GetChildPrefabs()) {
                if (baselineChild && baselineChild->GetName() == child->GetName()) {
                    return false;
                }
            }

            return true;
        }

        bool PrefabOverrideDiff::HasAnyComponentOverrides(
            const GameObject* gameObject,
            const Prefab* baselineNode)
        {
            if (!gameObject || !baselineNode) {
                return false;
            }

            for (const auto& comp : gameObject->GetComponents()) {
                if (!comp) {
                    continue;
                }

                if (IsAddedComponent(comp.get(), baselineNode)) {
                    return true;
                }

                const ComponentSnapshot* baselineSnap = FindBaselineSnapshot(
                    baselineNode,
                    comp->GetTypeName());
                if (!GetOverriddenProperties(comp.get(), baselineSnap).empty()) {
                    return true;
                }
            }

            return false;
        }

        bool PrefabOverrideDiff::HasAnyOverrides(
            const GameObject* gameObject,
            const Prefab* baselineNode)
        {
            if (!gameObject || !baselineNode) {
                return false;
            }

            if (IsTransformOverridden(gameObject, baselineNode) ||
                IsActiveOverridden(gameObject) ||
                IsCollisionLayerOverridden(gameObject, baselineNode) ||
                HasAnyComponentOverrides(gameObject, baselineNode)) {
                return true;
            }

            for (GameObject* child : gameObject->GetChildren()) {
                if (!child || child->IsTransient()) {
                    continue;
                }

                if (IsSceneOnlyChild(child, baselineNode)) {
                    return true;
                }

                const Prefab* childBaseline = nullptr;
                for (const auto& baselineChild : baselineNode->GetChildPrefabs()) {
                    if (baselineChild && baselineChild->GetName() == child->GetName()) {
                        childBaseline = baselineChild.get();
                        break;
                    }
                }

                if (childBaseline && HasAnyOverrides(child, childBaseline)) {
                    return true;
                }
            }

            return false;
        }

        bool PrefabOverrideDiff::ShouldPersistPrefabChild(
            const GameObject* gameObject,
            const Prefab* baselineNode)
        {
            if (!gameObject || !baselineNode) {
                return true;
            }

            for (GameObject* child : gameObject->GetChildren()) {
                if (!child || child->IsTransient()) {
                    continue;
                }
                if (IsSceneOnlyChild(child, baselineNode)) {
                    return true;
                }
            }

            if (IsTransformOverridden(gameObject, baselineNode)) {
                return true;
            }

            if (!gameObject->GetComponents().empty() && baselineNode->GetSnapshots().empty()) {
                return true;
            }

            if (HasAnyComponentOverrides(gameObject, baselineNode)) {
                return true;
            }

            if (IsActiveOverridden(gameObject) || IsCollisionLayerOverridden(gameObject, baselineNode)) {
                return true;
            }

            for (GameObject* child : gameObject->GetChildren()) {
                if (!child || child->IsTransient()) {
                    continue;
                }

                const Prefab* childBaseline = nullptr;
                for (const auto& baselineChild : baselineNode->GetChildPrefabs()) {
                    if (baselineChild && baselineChild->GetName() == child->GetName()) {
                        childBaseline = baselineChild.get();
                        break;
                    }
                }

                if (childBaseline && ShouldPersistPrefabChild(child, childBaseline)) {
                    return true;
                }
            }

            return false;
        }

    }
}
