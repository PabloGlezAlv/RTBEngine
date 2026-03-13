#pragma once
#include <string>
#include <memory>

namespace RTBEngine {
    namespace ECS { class Prefab; }

    namespace Scripting {


        class PrefabLoader {
        public:
            PrefabLoader() = delete;

            static std::unique_ptr<ECS::Prefab> Load(const std::string& filePath);
        };

    }
}
