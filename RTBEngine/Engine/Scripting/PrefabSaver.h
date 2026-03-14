#pragma once
#include "../RTBEngineAPI.h"
#include <string>

namespace RTBEngine {
    namespace ECS { class Prefab; }

    namespace Scripting {

        class RTB_API PrefabSaver {
        public:
            PrefabSaver() = delete;

            static bool Save(const ECS::Prefab& prefab, const std::string& filePath);
        };

    }
}
