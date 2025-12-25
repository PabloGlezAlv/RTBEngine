#pragma once
#include "Scene.h"
#include <memory>
#include <string>
#include <functional>

namespace RTBEngine {
    namespace ECS {

        class SceneManager {
        public:
            static SceneManager& GetInstance();

            SceneManager(const SceneManager&) = delete;
            SceneManager& operator=(const SceneManager&) = delete;

            bool Initialize();
            void Shutdown();

            bool LoadScene(const std::string& path);
            void UnloadCurrentScene();

            Scene* GetActiveScene() const { return activeScene.get(); }
            const std::string& GetActiveScenePath() const { return activeScenePath; }
            bool HasActiveScene() const { return activeScene != nullptr; }

            void SetOnSceneLoaded(std::function<void(Scene*)> callback);
            void SetOnSceneUnloading(std::function<void(Scene*)> callback);

        private:
            SceneManager() = default;
            ~SceneManager() = default;

            std::unique_ptr<Scene> activeScene;
            std::string activeScenePath;

            std::function<void(Scene*)> onSceneLoaded;
            std::function<void(Scene*)> onSceneUnloading;
        };

    }
}
