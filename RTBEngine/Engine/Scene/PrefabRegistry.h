#pragma once
#include "../RTBEngineAPI.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>

namespace RTBEngine {
    namespace ECS {

        class Prefab;

        class RTB_API PrefabRegistry {
        public:
            static PrefabRegistry& GetInstance();

            PrefabRegistry(const PrefabRegistry&) = delete;
            PrefabRegistry& operator=(const PrefabRegistry&) = delete;

            void LoadAll(const std::string& directory);
            void Register(const std::string& filePath);
            void Unload(const std::string& name);
            void Reload(const std::string& name);
            void Clear();

            Prefab* Get(const std::string& name) const;
            Prefab* GetByPath(const std::string& filePath) const;
            bool Has(const std::string& name) const;

            std::function<void(const std::string&)> onPrefabChanged;

        private:
            PrefabRegistry();
            ~PrefabRegistry();

            std::unordered_map<std::string, std::unique_ptr<Prefab>> prefabs;
            std::unordered_map<std::string, std::string> nameToPaths;
            std::unordered_map<std::string, std::string> pathToName;
        };

    }
}
