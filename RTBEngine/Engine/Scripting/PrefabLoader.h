#pragma once
#include <string>
#include <memory>

namespace RTBEngine {
    namespace Scene { class Prefab; }

    namespace Scripting {


        class PrefabLoader {
        public:
            PrefabLoader() = delete;

            static std::unique_ptr<Scene::Prefab> Load(const std::string& filePath);
        };

    }
}
