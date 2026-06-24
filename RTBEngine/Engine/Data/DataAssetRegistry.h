#pragma once

#include "../RTBEngineAPI.h"
#include "DataAsset.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace RTBEngine {
    namespace Data {

        #pragma warning(push)
        #pragma warning(disable: 4251)
        class RTB_API DataAssetRegistry {
        public:
            static DataAssetRegistry& GetInstance();

            void RegisterType(
                const std::string& typeName,
                std::function<DataAsset*()> factory,
                std::function<void(DataAsset*)> destroyer);

            void UnregisterType(const std::string& typeName);

            DataAsset* Create(const std::string& typeName) const;
            void Destroy(const std::string& typeName, DataAsset* asset) const;

            bool IsDataAssetType(const std::string& typeName) const;
            bool HasType(const std::string& typeName) const;

        private:
            DataAssetRegistry() = default;

            struct TypeEntry {
                std::function<DataAsset*()> factory;
                std::function<void(DataAsset*)> destroyer;
            };

            std::unordered_map<std::string, TypeEntry> types;
        };
        #pragma warning(pop)

    }
}
