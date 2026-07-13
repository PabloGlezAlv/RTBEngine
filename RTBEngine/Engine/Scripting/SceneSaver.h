#pragma once
#include "../RTBEngineAPI.h"
#include <string>
#include <fstream>

namespace RTBEngine {
    namespace ECS {
        class Scene;
        class GameObject;
        class Prefab;
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
            static void WriteGameObject(std::ofstream& file, const ECS::GameObject* go, int indent,
                const ECS::Prefab* baselinePrefab = nullptr);
            static void WriteTransform(std::ofstream& file, const ECS::GameObject* go, int indent);
            static void WriteComponents(std::ofstream& file, const ECS::GameObject* go, int indent);
            static void WritePrefabInstance(std::ofstream& file, const ECS::GameObject* go, int indent);
            static void WritePrefabOverrides(std::ofstream& file, const ECS::GameObject* go, int indent);
            static void WritePrefabNodePersistence(std::ofstream& file, const ECS::GameObject* go, int indent,
                const ECS::Prefab* baselineNode);

        };

    }
}
