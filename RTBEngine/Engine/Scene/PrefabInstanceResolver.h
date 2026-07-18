#pragma once

#include "../RTBEngineAPI.h"
#include <string>
#include <vector>

namespace RTBEngine {
    namespace Scene {

        class GameObject;
        class Prefab;

        struct RTB_API PrefabInstanceContext {
            GameObject* target = nullptr;
            GameObject* instanceRoot = nullptr;
            std::string assetName;
            std::string assetFilePath;
            std::vector<std::string> nodePath;
            const Prefab* baselineNode = nullptr;

            bool IsValid() const { return instanceRoot != nullptr && baselineNode != nullptr; }
            bool IsInstanceRoot() const { return target != nullptr && target == instanceRoot; }
        };

        class RTB_API PrefabInstanceResolver {
        public:
            static PrefabInstanceContext Resolve(GameObject* gameObject);
            static const Prefab* FindBaselineChild(const Prefab* rootBaseline, const std::vector<std::string>& nodePath);
        };

    }
}
