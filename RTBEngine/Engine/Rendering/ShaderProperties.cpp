#include "ShaderProperties.h"
#include "ShaderAsset.h"
#include "Shader.h"
#include "../Core/ResourceManager.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace RTBEngine {
    namespace Rendering {

        namespace {

            void Trim(std::string& value)
            {
                const size_t start = value.find_first_not_of(" \t\r\n");
                const size_t end = value.find_last_not_of(" \t\r\n");
                value = (start == std::string::npos) ? "" : value.substr(start, end - start + 1);
            }

            bool FindOverrideValue(const std::string& blob,
                                   const std::string& uniformName,
                                   std::string& outValue)
            {
                size_t searchPos = 0;
                const std::string key = uniformName + '=';
                while (searchPos < blob.size()) {
                    const size_t entryEnd = blob.find('|', searchPos);
                    const std::string entry = blob.substr(
                        searchPos,
                        entryEnd == std::string::npos ? std::string::npos : entryEnd - searchPos);
                    if (entry.rfind(key, 0) == 0) {
                        outValue = entry.substr(key.size());
                        return true;
                    }
                    if (entryEnd == std::string::npos) {
                        break;
                    }
                    searchPos = entryEnd + 1;
                }
                return false;
            }

            void SetOverrideValue(std::string& blob,
                                  const std::string& uniformName,
                                  const std::string& value)
            {
                std::ostringstream rebuilt;
                bool replaced = false;
                size_t searchPos = 0;
                const std::string key = uniformName + '=';

                while (searchPos < blob.size()) {
                    const size_t entryEnd = blob.find('|', searchPos);
                    const std::string entry = blob.substr(
                        searchPos,
                        entryEnd == std::string::npos ? std::string::npos : entryEnd - searchPos);

                    if (entry.rfind(key, 0) != 0) {
                        if (!rebuilt.str().empty()) {
                            rebuilt << '|';
                        }
                        rebuilt << entry;
                    } else {
                        replaced = true;
                    }

                    if (entryEnd == std::string::npos) {
                        break;
                    }
                    searchPos = entryEnd + 1;
                }

                if (!replaced) {
                    if (!value.empty()) {
                        if (!rebuilt.str().empty()) {
                            rebuilt << '|';
                        }
                        rebuilt << key << value;
                    }
                } else if (!value.empty()) {
                    if (!rebuilt.str().empty()) {
                        rebuilt << '|';
                    }
                    rebuilt << key << value;
                }

                blob = rebuilt.str();
            }

            std::vector<float> ParseFloatComponents(const std::string& value)
            {
                std::vector<float> components;
                std::string current;
                for (char character : value) {
                    if (character == ',') {
                        Trim(current);
                        if (!current.empty()) {
                            components.push_back(std::stof(current));
                        }
                        current.clear();
                    } else {
                        current.push_back(character);
                    }
                }
                Trim(current);
                if (!current.empty()) {
                    components.push_back(std::stof(current));
                }
                return components;
            }

            std::string FormatFloatComponents(const Math::Vector4& value, int count)
            {
                std::ostringstream stream;
                stream << value.x;
                if (count > 1) {
                    stream << ',' << value.y;
                }
                if (count > 2) {
                    stream << ',' << value.z;
                }
                if (count > 3) {
                    stream << ',' << value.w;
                }
                return stream.str();
            }

            bool IsMaterialManagedUniform(const std::string& uniformName)
            {
                return uniformName == "uColor"
                    || uniformName == "uTexture"
                    || uniformName == "uDiffuseColor"
                    || uniformName == "uShininess"
                    || uniformName == "uHasTexture";
            }

            std::vector<ShaderPropertyDefinition> GetBuiltinDefinitions()
            {
                ShaderPropertyDefinition colorProperty;
                colorProperty.uniformName = "uColor";
                colorProperty.displayName = "Color";
                colorProperty.type = ShaderPropertyType::Color;

                ShaderPropertyDefinition textureProperty;
                textureProperty.uniformName = "uTexture";
                textureProperty.displayName = "Texture";
                textureProperty.type = ShaderPropertyType::Texture;

                return { colorProperty, textureProperty };
            }

        }

        std::vector<ShaderPropertyDefinition> ShaderProperties::GetDefinitions(const std::string& shaderRef)
        {
            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();
            const std::string normalizedRef = resources.TryMakeAssetRelativePath(shaderRef);
            const std::string lookupRef = normalizedRef.empty() ? shaderRef : normalizedRef;

            if (Core::ResourceManager::IsShaderAssetRef(lookupRef)) {
                const Rendering::ShaderAssetData* cachedData = nullptr;
                if (!resources.TryGetShaderAssetData(lookupRef, &cachedData)) {
                    resources.LoadShaderAsset(lookupRef);
                    resources.TryGetShaderAssetData(lookupRef, &cachedData);
                }

                if (cachedData && !cachedData->properties.empty()) {
                    return cachedData->properties;
                }

                Rendering::ShaderAssetData assetData;
                if (Rendering::ShaderAsset::ParseFile(lookupRef, assetData) && !assetData.properties.empty()) {
                    return assetData.properties;
                }
            }

            if (lookupRef.empty() || lookupRef == "basic") {
                return GetBuiltinDefinitions();
            }

            return {};
        }

        Math::Vector4 ShaderProperties::ResolveColorValue(
            const ShaderPropertyDefinition& definition,
            const Math::Vector4& colorRef,
            const std::string& overrideBlob)
        {
            if (definition.uniformName == "uColor") {
                return colorRef;
            }

            std::string overrideValue;
            if (FindOverrideValue(overrideBlob, definition.uniformName, overrideValue)) {
                const std::vector<float> components = ParseFloatComponents(overrideValue);
                Math::Vector4 resolved = definition.defaultValue;
                if (!components.empty()) {
                    resolved.x = components[0];
                }
                if (components.size() > 1) {
                    resolved.y = components[1];
                }
                if (components.size() > 2) {
                    resolved.z = components[2];
                }
                if (components.size() > 3) {
                    resolved.w = components[3];
                }
                return resolved;
            }

            return definition.defaultValue;
        }

        float ShaderProperties::ResolveFloatValue(
            const ShaderPropertyDefinition& definition,
            const std::string& overrideBlob)
        {
            std::string overrideValue;
            if (FindOverrideValue(overrideBlob, definition.uniformName, overrideValue)) {
                return std::stof(overrideValue);
            }
            return definition.defaultValue.x;
        }

        Math::Vector4 ShaderProperties::ResolveVectorValue(
            const ShaderPropertyDefinition& definition,
            const std::string& overrideBlob)
        {
            std::string overrideValue;
            if (FindOverrideValue(overrideBlob, definition.uniformName, overrideValue)) {
                const std::vector<float> components = ParseFloatComponents(overrideValue);
                Math::Vector4 resolved = definition.defaultValue;
                if (!components.empty()) {
                    resolved.x = components[0];
                }
                if (components.size() > 1) {
                    resolved.y = components[1];
                }
                if (components.size() > 2) {
                    resolved.z = components[2];
                }
                if (components.size() > 3) {
                    resolved.w = components[3];
                }
                return resolved;
            }
            return definition.defaultValue;
        }

        void ShaderProperties::SetColorOverride(std::string& overrideBlob,
                                                const std::string& uniformName,
                                                const Math::Vector4& value)
        {
            SetOverrideValue(overrideBlob, uniformName, FormatFloatComponents(value, 4));
        }

        void ShaderProperties::SetFloatOverride(std::string& overrideBlob,
                                                  const std::string& uniformName,
                                                  float value)
        {
            SetOverrideValue(overrideBlob, uniformName, std::to_string(value));
        }

        void ShaderProperties::SetVectorOverride(std::string& overrideBlob,
                                                 const std::string& uniformName,
                                                 const Math::Vector4& value,
                                                 int componentCount)
        {
            SetOverrideValue(overrideBlob, uniformName, FormatFloatComponents(value, componentCount));
        }

        void ShaderProperties::ApplyExtraUniforms(Shader* shader,
                                                  const std::string& shaderRef,
                                                  const std::string& overrideBlob,
                                                  const Math::Vector4& colorRef)
        {
            if (!shader) {
                return;
            }

            const std::vector<ShaderPropertyDefinition> definitions = GetDefinitions(shaderRef);
            for (const ShaderPropertyDefinition& definition : definitions) {
                if (IsMaterialManagedUniform(definition.uniformName)) {
                    continue;
                }

                switch (definition.type) {
                case ShaderPropertyType::Color: {
                    const Math::Vector4 colorValue = ResolveColorValue(definition, colorRef, overrideBlob);
                    shader->SetVector4(definition.uniformName, colorValue);
                    break;
                }
                case ShaderPropertyType::Float: {
                    shader->SetFloat(definition.uniformName, ResolveFloatValue(definition, overrideBlob));
                    break;
                }
                case ShaderPropertyType::Vector2: {
                    const Math::Vector4 vectorValue = ResolveVectorValue(definition, overrideBlob);
                    shader->SetVector2(definition.uniformName, Math::Vector2(vectorValue.x, vectorValue.y));
                    break;
                }
                case ShaderPropertyType::Vector3: {
                    const Math::Vector4 vectorValue = ResolveVectorValue(definition, overrideBlob);
                    shader->SetVector3(
                        definition.uniformName,
                        Math::Vector3(vectorValue.x, vectorValue.y, vectorValue.z));
                    break;
                }
                case ShaderPropertyType::Vector4: {
                    shader->SetVector4(
                        definition.uniformName,
                        ResolveVectorValue(definition, overrideBlob));
                    break;
                }
                case ShaderPropertyType::Texture:
                    break;
                }
            }
        }

    }
}
