#pragma once
#include "../RTBEngineAPI.h"
#include <string>
#include <fstream>
#include <unordered_set>

namespace RTBEngine {
    namespace Scene {
        class Scene;
        class GameObject;
        class Prefab;
    }
}

namespace RTBEngine {
    namespace Scripting {

        class RTB_API SceneSaver {
        public:
            static bool SaveScene(const Scene::Scene* scene, const std::string& filePath);

        private:
            static void WriteSceneHeader(std::ofstream& file, const Scene::Scene* scene);
            static void WriteGameObjects(std::ofstream& file, const Scene::Scene* scene);
            static void WriteGameObject(std::ofstream& file, const Scene::GameObject* go, int indent,
                const Scene::Prefab* baselinePrefab,
                std::unordered_set<const Scene::GameObject*>& visited);
            static void WriteTransform(std::ofstream& file, const Scene::GameObject* go, int indent);
            static void WriteComponents(std::ofstream& file, const Scene::GameObject* go, int indent);
            static void WritePrefabInstance(std::ofstream& file, const Scene::GameObject* go, int indent);
            static void WritePrefabOverrides(std::ofstream& file, const Scene::GameObject* go, int indent);
            static void WritePrefabNodePersistence(std::ofstream& file, const Scene::GameObject* go, int indent,
                const Scene::Prefab* baselineNode);

        };

    }
}
