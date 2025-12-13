#pragma once
#include "GameObject.h"
#include "../Rendering/Camera.h"
#include "../Rendering/Lighting/Light.h"
#include "LightComponent.h"
#include <vector>
#include <string>

namespace RTBEngine {
    namespace ECS {

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
            void Render(Rendering::Camera* camera);

            const std::string& GetName() const { return name; }
            void CollectLights();
            const std::vector<Rendering::Light*>& GetLights() const { return lights; }
            const std::vector<GameObject*>& GetGameObjects() const { return gameObjects; }

        private:
            std::string name;
            std::vector<GameObject*> gameObjects;
            std::vector<Rendering::Light*> lights;
        };

    }
}