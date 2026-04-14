#include "SceneManager.h"
#include "../Scripting/SceneLoader.h"
#include "../Core/Logger.h"
#include "Prefab.h"
#include <cstdio>

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

        bool SceneManager::Initialize() {
            return true;
        }

        void SceneManager::Shutdown() {
            UnloadCurrentScene();
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

            previousScene.reset();
            activeScene.reset(newScene);
            activeScenePath = path;
            sceneDirty = false;

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

            if (activeScene) {
                isSceneTransitioning = true;
                if (onSceneUnloading) {
                    onSceneUnloading(activeScene.get());
                }
                activeScene.reset();
                activeScenePath.clear();
                sceneDirty = false;
                isSceneTransitioning = false;
            }
        }

        GameObject* SceneManager::Instantiate(const std::string& name, GameObject* parent)
        {
            Scene* scene = GetActiveScene();
            if (!scene) {
                RTB_ERROR("SceneManager::Instantiate - No active scene");
                return nullptr;
            }
            auto* go = new GameObject(name);
            if (parent)
                go->SetParent(parent);
            scene->AddGameObject(go);
            return go;
        }

        GameObject* SceneManager::Instantiate(const Prefab& prefab, GameObject* parent)
        {
            return prefab.Instantiate(parent);
        }


        void SceneManager::SetOnSceneLoaded(std::function<void(Scene*)> callback) {
            onSceneLoaded = callback;
        }

        void SceneManager::SetOnSceneUnloading(std::function<void(Scene*)> callback) {
            onSceneUnloading = callback;
        }

        void SceneManager::MarkSceneDirty() {
            sceneDirty = true;
        }

        void SceneManager::ClearSceneDirty() {
            sceneDirty = false;
        }

    }
}
