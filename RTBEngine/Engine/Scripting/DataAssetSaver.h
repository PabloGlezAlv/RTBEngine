#pragma once

#include "../RTBEngineAPI.h"
#include <string>

namespace RTBEngine {
    namespace Data {
        class DataAsset;
    }

    namespace Scripting {

        class RTB_API DataAssetSaver {
        public:
            static bool Save(const std::string& filePath, const Data::DataAsset& asset);
        };

    }
}
