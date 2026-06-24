#pragma once

#include "../RTBEngineAPI.h"
#include <memory>
#include <string>

namespace RTBEngine {
    namespace Data {
        class DataAsset;
    }

    namespace Scripting {

        class RTB_API DataAssetLoader {
        public:
            static std::unique_ptr<Data::DataAsset> Load(const std::string& filePath);
        };

    }
}
