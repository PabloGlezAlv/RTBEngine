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
            // Unload current scene first
            if (activeScene) {
                if (onSceneUnloading) {
                    onSceneUnloading(activeScene.get());
                }
                activeScene.reset();
            }

            // Load new scene
            Scene* newScene = Scripting::SceneLoader::LoadScene(path);
            if (!newScene) {
                RTB_ERROR("SceneManager: Failed to load scene '" + path + "'");
                return false;
            }

            activeScene.reset(newScene);
            activeScenePath = path;

            // Notify listeners
            if (onSceneLoaded) {
                onSceneLoaded(activeScene.get());
            }

            return true;
        }

        void SceneManager::UnloadCurrentScene() {
            if (activeScene) {
                if (onSceneUnloading) {
                    onSceneUnloading(activeScene.get());
                }
                activeScene.reset();
                activeScenePath.clear();
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
