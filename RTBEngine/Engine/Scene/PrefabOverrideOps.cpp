#include "PrefabOverrideOps.h"
#include "PrefabOverrideDiff.h"
#include "Prefab.h"
#include "GameObject.h"
#include "Component.h"
#include "Transform.h"
#include "Scene.h"
#include "SceneManager.h"
#include "PrefabRegistry.h"
#include "../Scripting/PrefabSaver.h"
#include "../Core/Logger.h"
#include "../Math/Math.h"
#include <filesystem>
#include <string>
#include <vector>

namespace RTBEngine {
    namespace Scene {

        namespace {
            ComponentSnapshot* FindOrCreateMutableSnapshot(Prefab* baselineNode, const char* typeName)
            {
                if (!baselineNode || !typeName) {
                    return nullptr;
                }

                for (ComponentSnapshot& snap : baselineNode->GetMutableSnapshots()) {
                    if (snap.typeName == typeName) {
                        return &snap;
                    }
                }

                ComponentSnapshot snap;
                snap.typeName = typeName;
                baselineNode->GetMutableSnapshots().push_back(std::move(snap));
                return &baselineNode->GetMutableSnapshots().back();
            }

            bool SavePrefabAsset(const std::string& assetName, std::unique_ptr<Prefab> prefabData)
            {
                if (!prefabData) {
                    return false;
                }

                const std::string filePath = PrefabRegistry::GetInstance().GetFilePath(assetName);
                if (filePath.empty()) {
                    RTB_WARN("PrefabOverrideOps: Missing file path for prefab '" + assetName + "'");
                    return false;
                }

                if (!Scripting::PrefabSaver::Save(*prefabData, filePath)) {
                    return false;
                }

                return PrefabOverrideOps::ReloadAssetAndRefreshInstances(assetName);
            }

            void ValidateComponents(GameObject* gameObject)
            {
                if (!gameObject) {
                    return;
                }

                GameObject::ComponentIteration iteration(gameObject);
                for (std::size_t i = 0; i < iteration.Count(); ++i) {
                    if (Component* comp = iteration.At(i)) {
                        comp->OnValidate();
                    }
                }

                for (GameObject* child : gameObject->GetChildren()) {
                    if (child && !child->IsTransient()) {
                        ValidateComponents(child);
                    }
                }
            }

            void SyncNodeFromBaseline(
                GameObject* gameObject,
                const Prefab* baselineNode,
                Scene* scene,
                GameObject* instanceRoot,
                bool addMissingChildren = false)
            {
                if (!gameObject || !baselineNode) {
                    return;
                }

                if (!gameObject->IsAnimatorBone()) {
                    gameObject->GetTransform().SetPosition(baselineNode->GetPosition());
                    gameObject->GetTransform().SetRotation(baselineNode->GetRotation());
                    gameObject->GetTransform().SetScale(baselineNode->GetScale());
                }

                gameObject->SetActive(true);
                gameObject->SetCollisionLayer(baselineNode->GetCollisionLayer());
                gameObject->SetStaticFlags(baselineNode->GetStaticFlags());

                std::vector<Component*> componentsToRemove;
                {
                    GameObject::ComponentIteration iteration(gameObject);
                    for (std::size_t i = 0; i < iteration.Count(); ++i) {
                        Component* comp = iteration.At(i);
                        if (!comp) {
                            continue;
                        }

                        const ComponentSnapshot* baselineSnap = PrefabOverrideDiff::FindBaselineSnapshot(
                            baselineNode,
                            comp->GetTypeName());
                        if (!baselineSnap) {
                            componentsToRemove.push_back(comp);
                            continue;
                        }

                        Prefab::ApplySnapshot(comp, *baselineSnap);

                        const Reflection::TypeInfo* typeInfo = comp->GetTypeInfo();
                        if (typeInfo) {
                            for (const Reflection::PropertyInfo* prop : typeInfo->GetSerializableProperties()) {
                                if (prop->type == Reflection::PropertyType::GameObjectRef ||
                                    prop->type == Reflection::PropertyType::ComponentRef) {
                                    Prefab::ApplySnapshotProperty(
                                        comp,
                                        *baselineSnap,
                                        prop,
                                        scene,
                                        instanceRoot);
                                }
                            }
                        }

                        comp->OnValidate();
                    }
                }

                for (Component* comp : componentsToRemove) {
                    gameObject->RemoveComponent(comp);
                }

                std::vector<GameObject*> childrenToRemove;
                for (GameObject* child : gameObject->GetChildren()) {
                    if (!child || child->IsTransient()) {
                        continue;
                    }

                    if (PrefabOverrideDiff::IsSceneOnlyChild(child, baselineNode)) {
                        childrenToRemove.push_back(child);
                        continue;
                    }

                    const Prefab* childBaseline = nullptr;
                    for (const auto& baselineChild : baselineNode->GetChildPrefabs()) {
                        if (baselineChild && baselineChild->GetName() == child->GetName()) {
                            childBaseline = baselineChild.get();
                            break;
                        }
                    }

                    if (!childBaseline) {
                        continue;
                    }

                    if (child->IsPrefabInstance() &&
                        PrefabRegistry::GetInstance().Has(child->GetPrefabName())) {
                        const Prefab* nestedAsset = PrefabRegistry::GetInstance().Get(child->GetPrefabName());
                        SyncNodeFromBaseline(child, nestedAsset, scene, child, addMissingChildren);
                        if (childBaseline->IsNestedPrefabInstance()) {
                            if (childBaseline->IsPositionSpecified()) {
                                child->GetTransform().SetPosition(childBaseline->GetPosition());
                            }
                            if (childBaseline->IsRotationSpecified()) {
                                child->GetTransform().SetRotation(childBaseline->GetRotation());
                            }
                            if (childBaseline->IsScaleSpecified()) {
                                child->GetTransform().SetScale(childBaseline->GetScale());
                            }
                        }
                        continue;
                    }

                    SyncNodeFromBaseline(child, childBaseline, scene, instanceRoot, addMissingChildren);
                }

                for (GameObject* child : childrenToRemove) {
                    if (scene) {
                        scene->RemoveGameObject(child);
                    }
                }

                if (!addMissingChildren) {
                    return;
                }

                for (const auto& baselineChild : baselineNode->GetChildPrefabs()) {
                    if (!baselineChild) {
                        continue;
                    }

                    bool exists = false;
                    for (GameObject* child : gameObject->GetChildren()) {
                        if (child && child->GetName() == baselineChild->GetName()) {
                            exists = true;
                            break;
                        }
                    }
                    if (exists || !scene) {
                        continue;
                    }

                    std::vector<GameObject*> created;
                    GameObject* spawned = baselineChild->Instantiate(gameObject, created, false);
                    if (!spawned) {
                        continue;
                    }

                    scene->AddGameObject(spawned);
                    for (GameObject* createdChild : created) {
                        if (createdChild) {
                            scene->AddGameObject(createdChild);
                        }
                    }
                    scene->BringGameObjectToLife(spawned);
                }
            }

            struct CapturedPropertyOverride {
                std::string componentType;
                std::string propertyName;
                ComponentSnapshot snapshot;
            };

            struct CapturedNodeOverride {
                GameObject* target = nullptr;
                bool transformOverridden = false;
                Math::Vector3 position;
                Math::Quaternion rotation;
                Math::Vector3 scale = Math::Vector3(1.0f, 1.0f, 1.0f);
                std::vector<CapturedPropertyOverride> properties;
            };

            void CaptureNodeOverrides(
                GameObject* gameObject,
                const Prefab* baselineNode,
                std::vector<CapturedNodeOverride>& outCaptures)
            {
                if (!gameObject || !baselineNode) {
                    return;
                }

                CapturedNodeOverride captured;
                captured.target = gameObject;
                if (PrefabOverrideDiff::IsTransformOverridden(gameObject, baselineNode)) {
                    captured.transformOverridden = true;
                    captured.position = gameObject->GetTransform().GetPosition();
                    captured.rotation = gameObject->GetTransform().GetRotation();
                    captured.scale = gameObject->GetTransform().GetScale();
                }

                {
                    GameObject::ComponentIteration iteration(gameObject);
                    for (std::size_t i = 0; i < iteration.Count(); ++i) {
                        Component* comp = iteration.At(i);
                        if (!comp || PrefabOverrideDiff::IsAddedComponent(comp, baselineNode)) {
                            continue;
                        }

                        const ComponentSnapshot* baselineSnap = PrefabOverrideDiff::FindBaselineSnapshot(
                            baselineNode,
                            comp->GetTypeName());
                        const std::vector<const Reflection::PropertyInfo*> overridden =
                            PrefabOverrideDiff::GetOverriddenProperties(comp, baselineSnap);
                        for (const Reflection::PropertyInfo* prop : overridden) {
                            if (!prop) {
                                continue;
                            }

                            CapturedPropertyOverride propertyOverride;
                            propertyOverride.componentType = comp->GetTypeName();
                            propertyOverride.propertyName = prop->name;
                            Prefab::SnapshotProperty(propertyOverride.snapshot, comp, prop);
                            captured.properties.push_back(std::move(propertyOverride));
                        }
                    }
                }

                if (captured.transformOverridden || !captured.properties.empty()) {
                    outCaptures.push_back(std::move(captured));
                }

                for (GameObject* child : gameObject->GetChildren()) {
                    if (!child || child->IsTransient()) {
                        continue;
                    }

                    if (child->IsPrefabInstance() &&
                        PrefabRegistry::GetInstance().Has(child->GetPrefabName())) {
                        CaptureNodeOverrides(
                            child,
                            PrefabRegistry::GetInstance().Get(child->GetPrefabName()),
                            outCaptures);
                        continue;
                    }

                    const Prefab* childBaseline = nullptr;
                    for (const auto& baselineChild : baselineNode->GetChildPrefabs()) {
                        if (baselineChild && baselineChild->GetName() == child->GetName()) {
                            childBaseline = baselineChild.get();
                            break;
                        }
                    }
                    if (childBaseline) {
                        CaptureNodeOverrides(child, childBaseline, outCaptures);
                    }
                }
            }

            void RestoreCapturedOverrides(
                const CapturedNodeOverride& captured,
                Scene* scene,
                GameObject* instanceRoot)
            {
                if (!captured.target) {
                    return;
                }

                if (captured.transformOverridden && !captured.target->IsAnimatorBone()) {
                    captured.target->GetTransform().SetPosition(captured.position);
                    captured.target->GetTransform().SetRotation(captured.rotation);
                    captured.target->GetTransform().SetScale(captured.scale);
                }

                for (const CapturedPropertyOverride& propertyOverride : captured.properties) {
                    Component* component = nullptr;
                    for (std::size_t i = 0, count = captured.target->GetComponentCount(); i < count; ++i) {
                        Component* candidate = captured.target->GetComponentAt(i);
                        if (candidate && std::string(candidate->GetTypeName()) == propertyOverride.componentType) {
                            component = candidate;
                            break;
                        }
                    }
                    if (!component) {
                        continue;
                    }

                    const Reflection::TypeInfo* typeInfo = component->GetTypeInfo();
                    if (!typeInfo) {
                        continue;
                    }

                    const Reflection::PropertyInfo* property =
                        typeInfo->GetProperty(propertyOverride.propertyName);
                    if (!property) {
                        continue;
                    }

                    Prefab::ApplySnapshotProperty(
                        component,
                        propertyOverride.snapshot,
                        property,
                        scene,
                        instanceRoot);
                    component->OnValidate();
                }
            }

            void MarkSceneDirtyIfNeeded()
            {
                SceneManager::GetInstance().MarkSceneDirty();
            }
        }

        bool PrefabOverrideOps::IsPropertyOverridden(
            GameObject* gameObject,
            Component* component,
            const Reflection::PropertyInfo* property)
        {
            const PrefabInstanceContext context = PrefabInstanceResolver::Resolve(gameObject);
            if (!context.IsValid() || !component || !property) {
                return false;
            }

            if (PrefabOverrideDiff::IsAddedComponent(component, context.baselineNode)) {
                return true;
            }

            const ComponentSnapshot* baselineSnap = PrefabOverrideDiff::FindBaselineSnapshot(
                context.baselineNode,
                component->GetTypeName());
            return PrefabOverrideDiff::IsPropertyOverridden(component, baselineSnap, property);
        }

        bool PrefabOverrideOps::RevertProperty(
            GameObject* gameObject,
            Component* component,
            const Reflection::PropertyInfo* property)
        {
            const PrefabInstanceContext context = PrefabInstanceResolver::Resolve(gameObject);
            if (!context.IsValid() || !component || !property) {
                return false;
            }

            if (PrefabOverrideDiff::IsAddedComponent(component, context.baselineNode)) {
                return false;
            }

            const ComponentSnapshot* baselineSnap = PrefabOverrideDiff::FindBaselineSnapshot(
                context.baselineNode,
                component->GetTypeName());
            if (!baselineSnap) {
                return false;
            }

            Scene* scene = SceneManager::GetInstance().GetActiveScene();
            Prefab::ApplySnapshotProperty(
                component,
                *baselineSnap,
                property,
                scene,
                context.instanceRoot);
            MarkSceneDirtyIfNeeded();
            return true;
        }

        bool PrefabOverrideOps::ApplyProperty(
            GameObject* gameObject,
            Component* component,
            const Reflection::PropertyInfo* property)
        {
            const PrefabInstanceContext context = PrefabInstanceResolver::Resolve(gameObject);
            if (!context.IsValid() || !component || !property || context.assetFilePath.empty()) {
                return false;
            }

            const ComponentSnapshot* baselineSnap = PrefabOverrideDiff::FindBaselineSnapshot(
                context.baselineNode,
                component->GetTypeName());
            if (!PrefabOverrideDiff::IsPropertyOverridden(component, baselineSnap, property)) {
                return false;
            }

            const Prefab* currentAsset = PrefabRegistry::GetInstance().Get(context.assetName);
            if (!currentAsset) {
                return false;
            }

            std::unique_ptr<Prefab> updatedAsset = currentAsset->DeepClone();
            Prefab* targetNode = context.nodePath.empty()
                ? updatedAsset.get()
                : updatedAsset->FindMutableChildByPath(context.nodePath);
            if (!targetNode) {
                return false;
            }

            ComponentSnapshot* targetSnap = FindOrCreateMutableSnapshot(
                targetNode,
                component->GetTypeName());
            if (!targetSnap) {
                return false;
            }

            Prefab::SnapshotProperty(*targetSnap, component, property);
            return SavePrefabAsset(context.assetName, std::move(updatedAsset));
        }

        bool PrefabOverrideOps::RevertTransform(GameObject* gameObject)
        {
            const PrefabInstanceContext context = PrefabInstanceResolver::Resolve(gameObject);
            if (!context.IsValid() || !context.baselineNode || gameObject->IsAnimatorBone()) {
                return false;
            }

            gameObject->GetTransform().SetPosition(context.baselineNode->GetPosition());
            gameObject->GetTransform().SetRotation(context.baselineNode->GetRotation());
            gameObject->GetTransform().SetScale(context.baselineNode->GetScale());
            MarkSceneDirtyIfNeeded();
            return true;
        }

        bool PrefabOverrideOps::ApplyTransform(GameObject* gameObject)
        {
            const PrefabInstanceContext context = PrefabInstanceResolver::Resolve(gameObject);
            if (!context.IsValid() || gameObject->IsAnimatorBone()) {
                return false;
            }

            if (!PrefabOverrideDiff::IsTransformOverridden(gameObject, context.baselineNode)) {
                return false;
            }

            const Prefab* currentAsset = PrefabRegistry::GetInstance().Get(context.assetName);
            if (!currentAsset) {
                return false;
            }

            std::unique_ptr<Prefab> updatedAsset = currentAsset->DeepClone();
            Prefab* targetNode = context.nodePath.empty()
                ? updatedAsset.get()
                : updatedAsset->FindMutableChildByPath(context.nodePath);
            if (!targetNode) {
                return false;
            }

            const auto& transform = gameObject->GetTransform();
            targetNode->SetPosition(transform.GetPosition());
            targetNode->SetRotation(transform.GetRotation());
            targetNode->SetScale(transform.GetScale());
            return SavePrefabAsset(context.assetName, std::move(updatedAsset));
        }

        bool PrefabOverrideOps::RevertAddedComponent(GameObject* gameObject, Component* component)
        {
            const PrefabInstanceContext context = PrefabInstanceResolver::Resolve(gameObject);
            if (!context.IsValid() || !component || !gameObject) {
                return false;
            }

            if (!PrefabOverrideDiff::IsAddedComponent(component, context.baselineNode)) {
                return false;
            }

            gameObject->RemoveComponent(component);
            MarkSceneDirtyIfNeeded();
            return true;
        }

        bool PrefabOverrideOps::ApplyAddedComponent(GameObject* gameObject, Component* component)
        {
            const PrefabInstanceContext context = PrefabInstanceResolver::Resolve(gameObject);
            if (!context.IsValid() || !component) {
                return false;
            }

            if (!PrefabOverrideDiff::IsAddedComponent(component, context.baselineNode)) {
                return false;
            }

            const Prefab* currentAsset = PrefabRegistry::GetInstance().Get(context.assetName);
            if (!currentAsset) {
                return false;
            }

            std::unique_ptr<Prefab> updatedAsset = currentAsset->DeepClone();
            Prefab* targetNode = context.nodePath.empty()
                ? updatedAsset.get()
                : updatedAsset->FindMutableChildByPath(context.nodePath);
            if (!targetNode) {
                return false;
            }

            ComponentSnapshot snap;
            Prefab::SnapshotComponent(snap, component);
            targetNode->GetMutableSnapshots().push_back(std::move(snap));
            return SavePrefabAsset(context.assetName, std::move(updatedAsset));
        }

        bool PrefabOverrideOps::RevertComponent(GameObject* gameObject, const char* typeName)
        {
            if (!gameObject || !typeName) {
                return false;
            }

            Component* component = nullptr;
            for (std::size_t i = 0, count = gameObject->GetComponentCount(); i < count; ++i) {
                Component* candidate = gameObject->GetComponentAt(i);
                if (candidate && std::string(candidate->GetTypeName()) == typeName) {
                    component = candidate;
                    break;
                }
            }

            if (!component) {
                return false;
            }

            const PrefabInstanceContext context = PrefabInstanceResolver::Resolve(gameObject);
            if (!context.IsValid()) {
                return false;
            }

            if (PrefabOverrideDiff::IsAddedComponent(component, context.baselineNode)) {
                return RevertAddedComponent(gameObject, component);
            }

            const ComponentSnapshot* baselineSnap = PrefabOverrideDiff::FindBaselineSnapshot(
                context.baselineNode,
                typeName);
            if (!baselineSnap) {
                return false;
            }

            Scene* scene = SceneManager::GetInstance().GetActiveScene();
            Prefab::ApplySnapshot(component, *baselineSnap);

            const Reflection::TypeInfo* typeInfo = component->GetTypeInfo();
            if (typeInfo) {
                for (const Reflection::PropertyInfo* prop : typeInfo->GetSerializableProperties()) {
                    if (prop->type == Reflection::PropertyType::GameObjectRef ||
                        prop->type == Reflection::PropertyType::ComponentRef) {
                        Prefab::ApplySnapshotProperty(
                            component,
                            *baselineSnap,
                            prop,
                            scene,
                            context.instanceRoot);
                    }
                }
            }

            component->OnValidate();
            MarkSceneDirtyIfNeeded();
            return true;
        }

        bool PrefabOverrideOps::ApplyComponent(GameObject* gameObject, const char* typeName)
        {
            if (!gameObject || !typeName) {
                return false;
            }

            Component* component = nullptr;
            for (std::size_t i = 0, count = gameObject->GetComponentCount(); i < count; ++i) {
                Component* candidate = gameObject->GetComponentAt(i);
                if (candidate && std::string(candidate->GetTypeName()) == typeName) {
                    component = candidate;
                    break;
                }
            }

            if (!component) {
                return false;
            }

            if (PrefabOverrideDiff::IsAddedComponent(
                    component,
                    PrefabInstanceResolver::Resolve(gameObject).baselineNode)) {
                return ApplyAddedComponent(gameObject, component);
            }

            const PrefabInstanceContext context = PrefabInstanceResolver::Resolve(gameObject);
            if (!context.IsValid()) {
                return false;
            }

            const Prefab* currentAsset = PrefabRegistry::GetInstance().Get(context.assetName);
            if (!currentAsset) {
                return false;
            }

            std::unique_ptr<Prefab> updatedAsset = currentAsset->DeepClone();
            Prefab* targetNode = context.nodePath.empty()
                ? updatedAsset.get()
                : updatedAsset->FindMutableChildByPath(context.nodePath);
            if (!targetNode) {
                return false;
            }

            ComponentSnapshot snap;
            Prefab::SnapshotComponent(snap, component);

            bool replaced = false;
            for (ComponentSnapshot& existing : targetNode->GetMutableSnapshots()) {
                if (existing.typeName == snap.typeName) {
                    existing = std::move(snap);
                    replaced = true;
                    break;
                }
            }

            if (!replaced) {
                targetNode->GetMutableSnapshots().push_back(std::move(snap));
            }

            return SavePrefabAsset(context.assetName, std::move(updatedAsset));
        }

        bool PrefabOverrideOps::RevertAll(GameObject* gameObject, Scene* scene, GameObject** outReplacementRoot)
        {
            const PrefabInstanceContext context = PrefabInstanceResolver::Resolve(gameObject);
            if (!context.IsValid() || !scene) {
                return false;
            }

            if (context.IsInstanceRoot()) {
                const Prefab* asset = PrefabRegistry::GetInstance().Get(context.assetName);
                if (!asset) {
                    return false;
                }

                GameObject* parent = gameObject->GetParent();
                const std::string savedName = gameObject->GetName();
                const std::string savedUuid = gameObject->GetUUID();
                const bool savedActive = gameObject->IsActive();

                scene->RemoveGameObject(gameObject);

                std::vector<GameObject*> childGOs;
                GameObject* replacement = asset->Instantiate(parent, childGOs);
                replacement->SetName(savedName);
                replacement->SetPrefabName(context.assetName);
                replacement->SetUUID(savedUuid);
                replacement->SetActive(savedActive);

                scene->AddGameObject(replacement);
                for (GameObject* child : childGOs) {
                    if (child) {
                        scene->AddGameObject(child);
                    }
                }

                scene->BringGameObjectToLife(replacement);
                ValidateComponents(replacement);

                if (outReplacementRoot) {
                    *outReplacementRoot = replacement;
                }

                MarkSceneDirtyIfNeeded();
                return true;
            }

            SyncNodeFromBaseline(
                gameObject,
                context.baselineNode,
                scene,
                context.instanceRoot);
            MarkSceneDirtyIfNeeded();
            return true;
        }

        bool PrefabOverrideOps::ApplyAll(GameObject* gameObject)
        {
            const PrefabInstanceContext context = PrefabInstanceResolver::Resolve(gameObject);
            if (!context.IsValid()) {
                return false;
            }

            const Prefab* currentAsset = PrefabRegistry::GetInstance().Get(context.assetName);
            if (!currentAsset) {
                return false;
            }

            if (context.IsInstanceRoot()) {
                std::unique_ptr<Prefab> updated = Prefab::CreateFromGameObject(gameObject);
                if (!updated) {
                    return false;
                }

                Prefab::CopySourceUuidsFrom(*currentAsset, *updated);
                updated->SetSourceUuid(currentAsset->GetSourceUuid());
                return SavePrefabAsset(context.assetName, std::move(updated));
            }

            std::unique_ptr<Prefab> updatedAsset = currentAsset->DeepClone();
            std::vector<std::string> parentPath = context.nodePath;
            if (parentPath.empty()) {
                return false;
            }

            const std::string childName = parentPath.back();
            parentPath.pop_back();

            Prefab* parentNode = updatedAsset.get();
            if (!parentPath.empty()) {
                parentNode = updatedAsset->FindMutableChildByPath(parentPath);
            }
            if (!parentNode) {
                return false;
            }

            std::unique_ptr<Prefab> replacement = Prefab::CreateFromGameObject(gameObject);

            const Prefab* previousChild = context.baselineNode;
            if (previousChild) {
                Prefab::CopySourceUuidsFrom(*previousChild, *replacement);
                replacement->SetSourceUuid(previousChild->GetSourceUuid());
            }

            auto& children = parentNode->GetMutableChildPrefabs();
            bool replaced = false;
            for (auto& child : children) {
                if (child && child->GetName() == childName) {
                    child = std::move(replacement);
                    replaced = true;
                    break;
                }
            }

            if (!replaced) {
                children.push_back(std::move(replacement));
            }

            return SavePrefabAsset(context.assetName, std::move(updatedAsset));
        }

        bool PrefabOverrideOps::ReloadAssetAndRefreshInstances(const std::string& assetName)
        {
            if (assetName.empty()) {
                return false;
            }

            Scene* scene = SceneManager::GetInstance().GetActiveScene();
            const Prefab* oldAsset = PrefabRegistry::GetInstance().Get(assetName);

            std::vector<GameObject*> instances;
            std::vector<std::vector<CapturedNodeOverride>> captures;
            if (scene && oldAsset) {
                for (const auto& gameObject : scene->GetGameObjects()) {
                    if (gameObject && gameObject->GetPrefabName() == assetName) {
                        instances.push_back(gameObject.get());
                        std::vector<CapturedNodeOverride> captured;
                        CaptureNodeOverrides(gameObject.get(), oldAsset, captured);
                        captures.push_back(std::move(captured));
                    }
                }
            }

            PrefabRegistry::GetInstance().Reload(assetName);
            const Prefab* newAsset = PrefabRegistry::GetInstance().Get(assetName);
            if (!newAsset) {
                return false;
            }

            if (scene) {
                for (std::size_t i = 0; i < instances.size(); ++i) {
                    GameObject* instance = instances[i];
                    if (!instance) {
                        continue;
                    }

                    SyncNodeFromBaseline(instance, newAsset, scene, instance, true);
                    for (const CapturedNodeOverride& captured : captures[i]) {
                        RestoreCapturedOverrides(captured, scene, instance);
                    }
                    ValidateComponents(instance);
                }

                if (!instances.empty()) {
                    MarkSceneDirtyIfNeeded();
                }
            }

            return true;
        }

    }
}
