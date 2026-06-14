#include "SceneManager.h"

#include "../Core/Logger.h"
#include "../Scripting/SceneLoader.h"
#include "GameObject.h"
#include "Prefab.h"
#include "Scene.h"
#include "SceneLifecycle.h"

#include <vector>

namespace {
    RTBEngine::ECS::GameObject* FinalizeInstantiation(
        RTBEngine::ECS::Scene* scene,
        RTBEngine::ECS::GameObject* root,
        const std::vector<RTBEngine::ECS::GameObject*>& children,
        const std::function<void(RTBEngine::ECS::GameObject*)>& onHierarchyAdded)
    {
        if (!scene || !root) {
            return nullptr;
        }

        scene->AddGameObject(root, false);
        for (RTBEngine::ECS::GameObject* child : children) {
            if (child) {
                scene->AddGameObject(child, false);
            }
        }

        RTBEngine::ECS::SceneLifecycle::BringHierarchyToLife(scene, root);

        if (onHierarchyAdded) {
            onHierarchyAdded(root);
        }

        return root;
    }

    RTBEngine::ECS::GameObject* InstantiatePrefab(
        RTBEngine::ECS::Scene* scene,
        const RTBEngine::ECS::Prefab& prefab,
        RTBEngine::ECS::GameObject* parent,
        const RTBEngine::Math::Vector3* positionOverride,
        const RTBEngine::Math::Quaternion* rotationOverride,
        const std::function<void(RTBEngine::ECS::GameObject*)>& onHierarchyAdded)
    {
        if (!scene) {
            return nullptr;
        }

        std::vector<RTBEngine::ECS::GameObject*> children;
        RTBEngine::ECS::GameObject* root = prefab.Instantiate(parent, children);
        if (!root) {
            return nullptr;
        }

        if (positionOverride) {
            root->GetTransform().SetPosition(*positionOverride);
        }

        if (rotationOverride) {
            root->GetTransform().SetRotation(*rotationOverride);
        }

        return FinalizeInstantiation(scene, root, children, onHierarchyAdded);
    }

    void SetHierarchyActive(RTBEngine::ECS::GameObject* root, bool active)
    {
        if (!root) {
            return;
        }

        root->SetActive(active);
        for (RTBEngine::ECS::GameObject* child : root->GetChildren()) {
            SetHierarchyActive(child, active);
        }
    }
}

namespace RTBEngine {
    namespace ECS {

        SceneManager& SceneManager::GetInstance() {
            static SceneManager instance;
            return instance;
        }

        SceneManager::SceneManager() = default;

        SceneManager::~SceneManager() {
            Shutdown();
        }

        void SceneManager::BeginSceneUnload() {
            ++sceneUnloadDepth;
        }

        void SceneManager::EndSceneUnload() {
            if (sceneUnloadDepth > 0) {
                --sceneUnloadDepth;
            }
        }

        bool SceneManager::Initialize() {
            return true;
        }

        void SceneManager::Shutdown() {
            UnloadCurrentScene();
            onSceneLoaded = nullptr;
            onSceneUnloading = nullptr;
            onHierarchyAdded = nullptr;
            onHierarchyDeactivated = nullptr;
        }

        bool SceneManager::LoadScene(const std::string& path) {
            ClearPendingSceneLoad();

            if (path.empty()) {
                RTB_WARN("SceneManager: Ignoring empty scene path");
                return false;
            }

            if (isSceneTransitioning) {
                RTB_WARN("SceneManager: Ignoring LoadScene while another scene transition is active");
                return false;
            }

            isSceneTransitioning = true;

            std::unique_ptr<Scene> previousScene = std::move(activeScene);
            const std::string previousScenePath = activeScenePath;
            const bool previousSceneDirty = sceneDirty;

            activeScenePath.clear();
            sceneDirty = false;

            Scene* newScene = Scripting::SceneLoader::LoadScene(path);
            if (!newScene) {
                RTB_ERROR("SceneManager: Failed to load scene '" + path + "'");
                activeScene = std::move(previousScene);
                activeScenePath = previousScenePath;
                sceneDirty = previousSceneDirty;
                isSceneTransitioning = false;
                return false;
            }

            if (previousScene && onSceneUnloading) {
                onSceneUnloading(previousScene.get());
            }

            if (previousScene) {
                BeginSceneUnload();
                previousScene.reset();
                EndSceneUnload();
            }
            activeScene.reset(newScene);
            activeScenePath = path;
            sceneDirty = false;

            SceneLifecycle::BringSceneToLife(activeScene.get());

            if (onSceneLoaded) {
                onSceneLoaded(activeScene.get());
            }

            isSceneTransitioning = false;
            return true;
        }

        bool SceneManager::RequestSceneLoad(const char* path) {
            if (!path || path[0] == '\0') {
                RTB_WARN("SceneManager: Ignoring empty requested scene path");
                return false;
            }

            if (isSceneTransitioning) {
                RTB_WARN("SceneManager: Ignoring requested scene load during active scene transition");
                return false;
            }

            pendingScenePath = path;
            hasPendingSceneLoad = true;
            return true;
        }

        bool SceneManager::ProcessPendingSceneLoad() {
            if (!hasPendingSceneLoad) {
                return false;
            }

            const std::string requestedPath = pendingScenePath;
            ClearPendingSceneLoad();

            return LoadScene(requestedPath);
        }

        void SceneManager::ClearPendingSceneLoad() {
            pendingScenePath.clear();
            hasPendingSceneLoad = false;
        }

        void SceneManager::UnloadCurrentScene() {
            ClearPendingSceneLoad();

            if (isSceneTransitioning) {
                RTB_WARN("SceneManager: Ignoring UnloadCurrentScene while another scene transition is active");
                return;
            }

            if (!activeScene) {
                return;
            }

            isSceneTransitioning = true;
            if (onSceneUnloading) {
                onSceneUnloading(activeScene.get());
            }

            BeginSceneUnload();
            activeScene.reset();
            EndSceneUnload();
            activeScenePath.clear();
            sceneDirty = false;
            isSceneTransitioning = false;
        }

        GameObject* SceneManager::Instantiate(const std::string& name, GameObject* parent)
        {
            Scene* scene = GetActiveScene();
            if (!scene) {
                RTB_ERROR("SceneManager::Instantiate - No active scene");
                return nullptr;
            }

            GameObject* gameObject = new GameObject(name);
            if (parent) {
                gameObject->SetParent(parent);
            }

            const std::vector<GameObject*> noChildren;
            return FinalizeInstantiation(scene, gameObject, noChildren, onHierarchyAdded);
        }

        GameObject* SceneManager::Instantiate(const Prefab& prefab, GameObject* parent)
        {
            Scene* scene = GetActiveScene();
            if (!scene) {
                RTB_ERROR("SceneManager::Instantiate(Prefab) - No active scene");
                return nullptr;
            }

            return InstantiatePrefab(scene, prefab, parent, nullptr, nullptr, onHierarchyAdded);
        }

        GameObject* SceneManager::Instantiate(const Prefab& prefab,
                                              const Math::Vector3& position,
                                              const Math::Quaternion& rotation,
                                              GameObject* parent)
        {
            Scene* scene = GetActiveScene();
            if (!scene) {
                RTB_ERROR("SceneManager::Instantiate(Prefab, Transform) - No active scene");
                return nullptr;
            }

            return InstantiatePrefab(scene, prefab, parent, &position, &rotation, onHierarchyAdded);
        }

        void SceneManager::DeactivateHierarchy(GameObject* root)
        {
            if (!root) {
                return;
            }

            if (onHierarchyDeactivated) {
                onHierarchyDeactivated(root);
            }

            SetHierarchyActive(root, false);
        }

        void SceneManager::SetOnSceneLoaded(std::function<void(Scene*)> callback) {
            onSceneLoaded = callback;
        }

        void SceneManager::SetOnSceneUnloading(std::function<void(Scene*)> callback) {
            onSceneUnloading = callback;
        }

        void SceneManager::SetOnHierarchyAdded(std::function<void(GameObject*)> callback) {
            onHierarchyAdded = callback;
        }

        void SceneManager::SetOnHierarchyDeactivated(std::function<void(GameObject*)> callback) {
            onHierarchyDeactivated = callback;
        }

        void SceneManager::MarkSceneDirty() {
            sceneDirty = true;
        }

        void SceneManager::ClearSceneDirty() {
            sceneDirty = false;
        }
    }
}
