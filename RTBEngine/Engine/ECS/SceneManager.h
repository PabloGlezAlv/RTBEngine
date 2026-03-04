#pragma once
#include "../RTBEngineAPI.h"
#include "Scene.h"
#include <memory>
#include <string>
#include <functional>

namespace RTBEngine {
    namespace ECS {

        // C4251: STL members in DLL-exported class are safe here because
        // SceneManager is a singleton — clients never copy or directly access them.
        #pragma warning(push)
        #pragma warning(disable: 4251)
        class RTB_API SceneManager {
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

            void MarkSceneDirty();
            void ClearSceneDirty();
            bool IsSceneDirty() const { return sceneDirty; }

        private:
            SceneManager();
            ~SceneManager();

            std::unique_ptr<Scene> activeScene;
            std::string activeScenePath;
            bool sceneDirty = false;

            std::function<void(Scene*)> onSceneLoaded;
            std::function<void(Scene*)> onSceneUnloading;
        };
        #pragma warning(pop)

    }
}
