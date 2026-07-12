#pragma once

#include "../RTBEngineAPI.h"
#include <filesystem>
#include <string>
#include <vector>

#include "ShaderProperties.h"

namespace RTBEngine {
    namespace Rendering {

        struct ShaderAssetData {
            std::string vertexPath;
            std::string fragmentPath;
            std::vector<ShaderPropertyDefinition> properties;
        };

        class RTB_API ShaderAsset {
        public:
            static bool ParseFile(const std::string& assetPath, ShaderAssetData& outData);
            static bool SaveFile(const std::string& assetPath, const ShaderAssetData& data);
            static bool CreateTemplate(const std::filesystem::path& assetPath,
                                       const std::filesystem::path& assetRoot);
        };

    }
}
