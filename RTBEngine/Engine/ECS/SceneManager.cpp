#include "SceneManager.h"
#include "../Scripting/SceneLoader.h"
#include <cstdio>
#include "../RTBEngine.h"

namespace RTBEngine {
    namespace ECS {

        SceneManager& SceneManager::GetInstance() {
            static SceneManager instance;
            return instance;
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

        void SceneManager::SetOnSceneLoaded(std::function<void(Scene*)> callback) {
            onSceneLoaded = callback;
        }

        void SceneManager::SetOnSceneUnloading(std::function<void(Scene*)> callback) {
            onSceneUnloading = callback;
        }

    }
}
