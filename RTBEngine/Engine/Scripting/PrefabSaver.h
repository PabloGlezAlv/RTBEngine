#pragma once
#include <string>

namespace RTBEngine {
    namespace ECS { class Prefab; }

    namespace Scripting {

        class PrefabSaver {
        public:
            PrefabSaver() = delete;

            static bool Save(const ECS::Prefab& prefab, const std::string& filePath);
        };

    }
}
