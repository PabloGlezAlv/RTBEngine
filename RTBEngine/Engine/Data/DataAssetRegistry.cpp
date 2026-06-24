#include "DataAssetRegistry.h"

#include "../Core/Logger.h"

namespace RTBEngine {
    namespace Data {

        DataAssetRegistry& DataAssetRegistry::GetInstance()
        {
            static DataAssetRegistry instance;
            return instance;
        }

        void DataAssetRegistry::RegisterType(
            const std::string& typeName,
            std::function<DataAsset*()> factory,
            std::function<void(DataAsset*)> destroyer)
        {
            if (typeName.empty() || !factory || !destroyer) {
                return;
            }

            types[typeName] = TypeEntry{ std::move(factory), std::move(destroyer) };
        }

        void DataAssetRegistry::UnregisterType(const std::string& typeName)
        {
            types.erase(typeName);
        }

        DataAsset* DataAssetRegistry::Create(const std::string& typeName) const
        {
            const auto it = types.find(typeName);
            if (it == types.end()) {
                RTB_ERROR("DataAssetRegistry: Data asset type '" + typeName + "' is not registered.");
                return nullptr;
            }

            return it->second.factory();
        }

        void DataAssetRegistry::Destroy(const std::string& typeName, DataAsset* asset) const
        {
            if (!asset) {
                return;
            }

            const auto it = types.find(typeName);
            if (it != types.end()) {
                it->second.destroyer(asset);
                return;
            }

            delete asset;
        }

        bool DataAssetRegistry::IsDataAssetType(const std::string& typeName) const
        {
            return types.find(typeName) != types.end();
        }

        bool DataAssetRegistry::HasType(const std::string& typeName) const
        {
            return IsDataAssetType(typeName);
        }

    }
}
