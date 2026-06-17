#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector3.h"
#include <cstdint>
#include <string>
#include <vector>

namespace RTBEngine {
    namespace ECS {
        class Scene;
    }

    namespace Navigation {

        struct NavMeshGridRecord {
            std::string ownerUuid;
            Math::Vector3 localOrigin = Math::Vector3(0.0f, 0.0f, 0.0f);
            Math::Vector3 gridSize = Math::Vector3(32.0f, 0.0f, 32.0f);
            float cellSize = 0.5f;
            int gridWidth = 0;
            int gridHeight = 0;
            std::vector<uint8_t> walkable;
        };

        class RTB_API NavMeshFile {
        public:
            static std::string GetNavMeshPathForScene(const std::string& sceneAssetPath);
            static bool SaveSceneNavMesh(const std::string& sceneAssetPath, ECS::Scene* scene);
            static bool LoadSceneNavMesh(const std::string& sceneAssetPath, ECS::Scene* scene);
            static bool DeleteNavMeshFile(const std::string& sceneAssetPath);
        };

    }
}
