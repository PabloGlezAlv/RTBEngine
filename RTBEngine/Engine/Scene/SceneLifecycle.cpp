#include "SceneLifecycle.h"

#include "Component.h"
#include "GameObject.h"
#include "Scene.h"
#include "../Scripting/SceneLoader.h"

#include <unordered_set>

namespace RTBEngine {
    namespace Scene {

        namespace {

            void CollectHierarchyPreOrderImpl(
                GameObject* root,
                std::vector<GameObject*>& outHierarchy,
                std::unordered_set<GameObject*>& visited)
            {
                if (!root || !visited.insert(root).second) {
                    return;
                }

                outHierarchy.push_back(root);
                for (GameObject* child : root->GetChildren()) {
                    CollectHierarchyPreOrderImpl(child, outHierarchy, visited);
                }
            }

            void MarkHierarchyInitialized(const std::vector<GameObject*>& hierarchy)
            {
                for (GameObject* gameObject : hierarchy) {
                    if (gameObject) {
                        gameObject->SetLifecycleInitialized(true);
                    }
                }
            }

        }

        void SceneLifecycle::CollectHierarchyPreOrder(
            GameObject* root,
            std::vector<GameObject*>& outHierarchy,
            bool skipAlreadyCollected)
        {
            if (!root) {
                return;
            }

            std::unordered_set<GameObject*> visited;
            if (skipAlreadyCollected) {
                for (GameObject* existing : outHierarchy) {
                    if (existing) {
                        visited.insert(existing);
                    }
                }
            }

            CollectHierarchyPreOrderImpl(root, outHierarchy, visited);
        }

        void SceneLifecycle::CollectSceneHierarchyPreOrder(Scene* scene, std::vector<GameObject*>& outHierarchy)
        {
            if (!scene) {
                return;
            }

            std::unordered_set<GameObject*> nodesInScene;
            for (const auto& gameObject : scene->GetGameObjects()) {
                if (gameObject) {
                    nodesInScene.insert(gameObject.get());
                }
            }

            std::unordered_set<GameObject*> visited;
            for (const auto& gameObject : scene->GetGameObjects()) {
                GameObject* candidate = gameObject.get();
                if (!candidate) {
                    continue;
                }

                GameObject* parent = candidate->GetParent();
                const bool parentInScene =
                    parent && nodesInScene.find(parent) != nodesInScene.end();
                if (parentInScene) {
                    continue;
                }

                CollectHierarchyPreOrderImpl(candidate, outHierarchy, visited);
            }
        }

        void SceneLifecycle::InvokeAwake(const std::vector<GameObject*>& hierarchy)
        {
            for (GameObject* gameObject : hierarchy) {
                if (!gameObject) {
                    continue;
                }

                GameObject::ComponentIteration iteration(gameObject);
                for (std::size_t i = 0; i < iteration.Count(); ++i) {
                    if (Component* component = iteration.At(i)) {
                        component->InvokeAwakeIfNeeded();
                    }
                }
            }
        }

        void SceneLifecycle::InvokeValidate(const std::vector<GameObject*>& hierarchy)
        {
            for (GameObject* gameObject : hierarchy) {
                if (!gameObject) {
                    continue;
                }

                GameObject::ComponentIteration iteration(gameObject);
                for (std::size_t i = 0; i < iteration.Count(); ++i) {
                    if (Component* component = iteration.At(i)) {
                        component->OnValidate();
                    }
                }
            }
        }

        void SceneLifecycle::InvokeAwakeAndValidate(Component* component)
        {
            if (!component) {
                return;
            }

            component->InvokeAwakeIfNeeded();
            component->OnValidate();
        }

        void SceneLifecycle::BringHierarchyToLife(Scene* scene, GameObject* root)
        {
            if (!scene || !root) {
                return;
            }

            for (;;) {
                std::vector<GameObject*> hierarchy;
                CollectHierarchyPreOrder(root, hierarchy);

                std::vector<GameObject*> pending;
                pending.reserve(hierarchy.size());
                for (GameObject* gameObject : hierarchy) {
                    if (gameObject && !gameObject->IsLifecycleInitialized()) {
                        pending.push_back(gameObject);
                    }
                }

                if (pending.empty()) {
                    break;
                }

                InvokeAwake(pending);
                InvokeValidate(pending);
                MarkHierarchyInitialized(pending);
                for (GameObject* gameObject : pending) {
                    if (gameObject) {
                        gameObject->SyncEnabledState();
                    }
                }
            }
        }

        void SceneLifecycle::BringSceneToLife(Scene* scene)
        {
            if (!scene) {
                return;
            }

            Scripting::SceneLoader::RebuildFbxHierarchies(scene);

            for (;;) {
                std::unordered_set<GameObject*> nodesInScene;
                for (const auto& gameObject : scene->GetGameObjects()) {
                    if (gameObject) {
                        nodesInScene.insert(gameObject.get());
                    }
                }

                std::vector<GameObject*> pendingRoots;
                for (const auto& gameObject : scene->GetGameObjects()) {
                    GameObject* candidate = gameObject.get();
                    if (!candidate || candidate->IsLifecycleInitialized()) {
                        continue;
                    }

                    GameObject* parent = candidate->GetParent();
                    const bool parentInScene =
                        parent && nodesInScene.find(parent) != nodesInScene.end();
                    const bool parentPending =
                        parentInScene && parent && !parent->IsLifecycleInitialized();
                    if (!parentPending) {
                        pendingRoots.push_back(candidate);
                    }
                }

                if (pendingRoots.empty()) {
                    break;
                }

                std::vector<GameObject*> hierarchy;
                for (GameObject* root : pendingRoots) {
                    CollectHierarchyPreOrder(root, hierarchy, true);
                }

                std::vector<GameObject*> pending;
                pending.reserve(hierarchy.size());
                for (GameObject* gameObject : hierarchy) {
                    if (gameObject && !gameObject->IsLifecycleInitialized()) {
                        pending.push_back(gameObject);
                    }
                }

                if (pending.empty()) {
                    break;
                }

                InvokeAwake(pending);
                InvokeValidate(pending);
                MarkHierarchyInitialized(pending);
                for (GameObject* gameObject : pending) {
                    if (gameObject) {
                        gameObject->SyncEnabledState();
                    }
                }
            }

            scene->SetLifecycleComplete(true);
        }

        void SceneLifecycle::ResetStartForHierarchy(GameObject* root)
        {
            if (!root) {
                return;
            }

            std::vector<GameObject*> hierarchy;
            CollectHierarchyPreOrder(root, hierarchy);

            for (GameObject* gameObject : hierarchy) {
                if (!gameObject) {
                    continue;
                }

                GameObject::ComponentIteration iteration(gameObject);
                for (std::size_t i = 0; i < iteration.Count(); ++i) {
                    if (Component* component = iteration.At(i)) {
                        component->ResetStartInvocation();
                    }
                }
            }
        }

        void SceneLifecycle::InvokeStartForHierarchy(GameObject* root)
        {
            // Prefab spawns during Play need OnStart immediately (Awake already ran in BringHierarchyToLife).
            if (!root) {
                return;
            }

            std::vector<GameObject*> hierarchy;
            CollectHierarchyPreOrder(root, hierarchy);

            for (GameObject* gameObject : hierarchy) {
                if (!gameObject || !gameObject->IsLifecycleInitialized()) {
                    continue;
                }

                GameObject::ComponentIteration iteration(gameObject);
                for (std::size_t i = 0; i < iteration.Count(); ++i) {
                    if (Component* component = iteration.At(i)) {
                        component->TryInvokeStart();
                    }
                }
            }
        }

    }
}
