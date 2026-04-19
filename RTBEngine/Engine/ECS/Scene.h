#pragma once
#include "../RTBEngineAPI.h"
#include "GameObject.h"
#include "../Rendering/Camera.h"
#include "../Rendering/Lighting/Light.h"
#include "LightComponent.h"
#include <vector>
#include <memory>
#include <string>
#include <cstdint>

namespace RTBEngine {
    namespace Rendering {
        class Cubemap;
    }
}


namespace RTBEngine {
    namespace ECS {

        class CameraComponent;

        #pragma warning(push)
        #pragma warning(disable: 4251)
        class RTB_API Scene {
        public:
            Scene(const std::string& name = "Untitled Scene");
            ~Scene();

            Scene(const Scene&) = delete;
            Scene& operator=(const Scene&) = delete;

            void AddGameObject(GameObject* gameObject);
            void RemoveGameObject(GameObject* gameObject);
            GameObject* FindGameObject(const std::string& name);
            GameObject* FindGameObjectByUUID(const std::string& uuid);

            void Update(float deltaTime);
            void FixedUpdate(float fixedDeltaTime);
            void LateUpdate(float deltaTime);
            void Render(Rendering::Camera* camera);

            // Skybox management (per-scene override)
            void SetSkyboxCubemap(Rendering::Cubemap* cubemap);
            Rendering::Cubemap* GetSkyboxCubemap() const { return skyboxCubemap; }
            void SetSkyboxEnabled(bool enabled) { skyboxEnabled = enabled; }
            bool IsSkyboxEnabled() const { return skyboxEnabled; }

            //Stats counters
            uint32_t GetActiveGameObjectCount() const;
            uint32_t GetActiveComponentCount() const;

            const std::string& GetName() const { return name; }
            void SetName(const std::string& newName) { name = newName; }
            void CollectLights();
            const std::vector<Rendering::Light*>& GetLights() const { return lights; }
            const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return gameObjects; }

            // Camera management
            void SetMainCamera(CameraComponent* camera);
            CameraComponent* GetMainCamera() const;
            Rendering::Camera* GetActiveCamera();

        private:
            //Deferred command buffer
            void FlushPendingCommands();

            std::string name;
            std::vector<std::unique_ptr<GameObject>> gameObjects;
            std::vector<Rendering::Light*> lights;

            std::vector<std::unique_ptr<GameObject>> pendingAdds;
            std::vector<GameObject*> pendingRemoves;
            int iterationDepth = 0;

            CameraComponent* mainCamera = nullptr;

            // Skybox settings
            Rendering::Cubemap* skyboxCubemap = nullptr;
            bool skyboxEnabled = true;

            bool pendingRenderLog = false;
        };
        #pragma warning(pop)

    }
}
