#pragma once

#include "../Math/Quaternions/Quaternion.h"
#include "../Math/Vectors/Vector3.h"
#include "../RTBEngineAPI.h"
#include "Scene.h"
#include <functional>
#include <memory>
#include <string>

namespace RTBEngine {
    namespace Scene {
        class GameObject;
        class Prefab;
        class Scene;

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
            bool RequestSceneLoad(const char* path);
            bool ProcessPendingSceneLoad();
            void ClearPendingSceneLoad();
            void UnloadCurrentScene();

            Scene* GetActiveScene() const { return activeScene.get(); }
            const std::string& GetActiveScenePath() const { return activeScenePath; }
            void SetActiveScenePath(const std::string& path) { activeScenePath = path; }
            bool HasActiveScene() const { return activeScene != nullptr; }

            void SetOnSceneLoaded(std::function<void(Scene*)> callback);
            void SetOnSceneUnloading(std::function<void(Scene*)> callback);
            void SetOnHierarchyAdded(std::function<void(GameObject*)> callback);
            void SetOnHierarchyDeactivated(std::function<void(GameObject*)> callback);

            GameObject* Instantiate(const std::string& name = "GameObject", GameObject* parent = nullptr);
            GameObject* Instantiate(const Prefab& prefab, GameObject* parent = nullptr, bool regenerateUuids = true);
            GameObject* Instantiate(const Prefab& prefab,
                                    const Math::Vector3& position,
                                    const Math::Quaternion& rotation,
                                    GameObject* parent = nullptr,
                                    bool regenerateUuids = true);
            void DeactivateHierarchy(GameObject* root);

            void MarkSceneDirty();
            void ClearSceneDirty();
            bool IsSceneDirty() const { return sceneDirty; }
            bool IsSceneUnloading() const { return sceneUnloadDepth > 0; }

        private:
            SceneManager();
            ~SceneManager();

            void BeginSceneUnload();
            void EndSceneUnload();

            std::unique_ptr<Scene> activeScene;
            std::string activeScenePath;
            std::string pendingScenePath;
            bool sceneDirty = false;
            bool hasPendingSceneLoad = false;
            bool isSceneTransitioning = false;
            int sceneUnloadDepth = 0;

            std::function<void(Scene*)> onSceneLoaded;
            std::function<void(Scene*)> onSceneUnloading;
            std::function<void(GameObject*)> onHierarchyAdded;
            std::function<void(GameObject*)> onHierarchyDeactivated;
        };
        #pragma warning(pop)
    }
}
