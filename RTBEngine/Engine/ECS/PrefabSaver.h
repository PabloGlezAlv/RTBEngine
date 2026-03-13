#pragma once
#include <string>

namespace RTBEngine {
    namespace ECS {

        class Prefab;

        class PrefabSaver {
        public:
            PrefabSaver() = delete;

            static bool Save(const Prefab& prefab, const std::string& filePath);
        };

    }
}
