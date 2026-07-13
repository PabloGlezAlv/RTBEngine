#include "PrefabOverrideOps.h"
#include "PrefabOverrideDiff.h"
#include "GameObject.h"
#include "Component.h"
#include "Scene.h"
#include "SceneManager.h"
#include "PrefabRegistry.h"
#include "../Scripting/PrefabSaver.h"
#include "../Scripting/ComponentRegistry.h"
#include "../Core/Logger.h"
#include <filesystem>
#include <vector>

namespace RTBEngine {
    namespace ECS {

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

                PrefabRegistry::GetInstance().Reload(assetName);
                return true;
            }

            void ValidateComponents(GameObject* gameObject)
            {
                if (!gameObject) {
                    return;
                }

                for (const auto& comp : gameObject->GetComponents()) {
                    if (comp) {
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
                GameObject* instanceRoot)
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

                std::vector<Component*> componentsToRemove;
                for (const auto& comp : gameObject->GetComponents()) {
                    if (!comp) {
                        continue;
                    }

                    const ComponentSnapshot* baselineSnap = PrefabOverrideDiff::FindBaselineSnapshot(
                        baselineNode,
                        comp->GetTypeName());
                    if (!baselineSnap) {
                        componentsToRemove.push_back(comp.get());
                        continue;
                    }

                    Prefab::ApplySnapshot(comp.get(), *baselineSnap);

                    const Reflection::TypeInfo* typeInfo = comp->GetTypeInfo();
                    if (typeInfo) {
                        for (const Reflection::PropertyInfo* prop : typeInfo->GetSerializableProperties()) {
                            if (prop->type == Reflection::PropertyType::GameObjectRef ||
                                prop->type == Reflection::PropertyType::ComponentRef) {
                                Prefab::ApplySnapshotProperty(
                                    comp.get(),
                                    *baselineSnap,
                                    prop,
                                    scene,
                                    instanceRoot);
                            }
                        }
                    }

                    comp->OnValidate();
                }

                for (Component* comp : componentsToRemove) {
                    gameObject->RemoveComponent(comp);
                    Scripting::ComponentRegistry::GetInstance().DestroyComponent(
                        comp->GetTypeName(),
                        comp);
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

                    if (childBaseline) {
                        SyncNodeFromBaseline(child, childBaseline, scene, instanceRoot);
                    }
                }

                for (GameObject* child : childrenToRemove) {
                    if (scene) {
                        scene->RemoveGameObject(child);
                    }
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

            const char* typeName = component->GetTypeName();
            gameObject->RemoveComponent(component);
            Scripting::ComponentRegistry::GetInstance().DestroyComponent(typeName, component);
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
            for (const auto& comp : gameObject->GetComponents()) {
                if (comp && std::string(comp->GetTypeName()) == typeName) {
                    component = comp.get();
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
            for (const auto& comp : gameObject->GetComponents()) {
                if (comp && std::string(comp->GetTypeName()) == typeName) {
                    component = comp.get();
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

    }
}
