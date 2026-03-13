#pragma once
#include "../RTBEngineAPI.h"
#include <string>
#include <fstream>

namespace RTBEngine {
    namespace ECS {
        class Scene;
        class GameObject;
    }
}

namespace RTBEngine {
    namespace Scripting {

        class RTB_API SceneSaver {
        public:
            static bool SaveScene(const ECS::Scene* scene, const std::string& filePath);

        private:
            static void WriteSceneHeader(std::ofstream& file, const ECS::Scene* scene);
            static void WriteGameObjects(std::ofstream& file, const ECS::Scene* scene);
            static void WriteGameObject(std::ofstream& file, const ECS::GameObject* go, int indent);
            static void WriteTransform(std::ofstream& file, const ECS::GameObject* go, int indent);
            static void WriteComponents(std::ofstream& file, const ECS::GameObject* go, int indent);
        };

    }
}
