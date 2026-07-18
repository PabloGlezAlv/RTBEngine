#pragma once
#include "../RTBEngineAPI.h"
#include <string>

namespace RTBEngine {
    namespace Scene { class Prefab; }

    namespace Scripting {

        class RTB_API PrefabSaver {
        public:
            PrefabSaver() = delete;

            static bool Save(const Scene::Prefab& prefab, const std::string& filePath);
        };

    }
}
