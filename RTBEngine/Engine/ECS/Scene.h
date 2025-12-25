#pragma once
#include "GameObject.h"
#include "../Rendering/Camera.h"
#include "../Rendering/Lighting/Light.h"
#include "LightComponent.h"
#include <vector>
#include <memory>
#include <string>

namespace RTBEngine {
    namespace ECS {

        class CameraComponent;

        class Scene {
        public:
            Scene(const std::string& name = "Untitled Scene");
            ~Scene();

            Scene(const Scene&) = delete;
            Scene& operator=(const Scene&) = delete;

            void AddGameObject(GameObject* gameObject);
            void RemoveGameObject(GameObject* gameObject);
            GameObject* FindGameObject(const std::string& name);

            void Update(float deltaTime);
            void FixedUpdate(float fixedDeltaTime);
            void Render(Rendering::Camera* camera);

            const std::string& GetName() const { return name; }
            void CollectLights();
            const std::vector<Rendering::Light*>& GetLights() const { return lights; }
            const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return gameObjects; }

            // Camera management
            void SetMainCamera(CameraComponent* camera);
            CameraComponent* GetMainCamera() const;
            Rendering::Camera* GetActiveCamera();
        private:
            std::string name;
            std::vector<std::unique_ptr<GameObject>> gameObjects;
            std::vector<Rendering::Light*> lights;
            
            CameraComponent* mainCamera = nullptr;
        };

    }
}