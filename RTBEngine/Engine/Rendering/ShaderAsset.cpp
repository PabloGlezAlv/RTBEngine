#include "ShaderAsset.h"

#include "../Core/Logger.h"
#include "../Core/ResourceManager.h"

#include <fstream>
#include <sstream>

namespace RTBEngine {
    namespace Rendering {

        namespace {
            void Trim(std::string& value)
            {
                const size_t start = value.find_first_not_of(" \t\r\n");
                const size_t end = value.find_last_not_of(" \t\r\n");
                value = (start == std::string::npos) ? "" : value.substr(start, end - start + 1);
            }

            void StripUtf8Bom(std::string& value)
            {
                if (value.size() >= 3
                    && static_cast<unsigned char>(value[0]) == 0xEF
                    && static_cast<unsigned char>(value[1]) == 0xBB
                    && static_cast<unsigned char>(value[2]) == 0xBF) {
                    value.erase(0, 3);
                }
            }

            std::vector<std::string> Split(const std::string& value, char delimiter)
            {
                std::vector<std::string> parts;
                std::string current;
                for (char character : value) {
                    if (character == delimiter) {
                        Trim(current);
                        parts.push_back(current);
                        current.clear();
                    } else {
                        current.push_back(character);
                    }
                }
                Trim(current);
                parts.push_back(current);
                return parts;
            }

            std::string FormatPropertyType(ShaderPropertyType type)
            {
                switch (type) {
                case ShaderPropertyType::Color: return "color";
                case ShaderPropertyType::Float: return "float";
                case ShaderPropertyType::Vector2: return "vec2";
                case ShaderPropertyType::Vector3: return "vec3";
                case ShaderPropertyType::Vector4: return "vec4";
                case ShaderPropertyType::Texture: return "texture";
                default: return "color";
                }
            }

            bool TryParsePropertyType(const std::string& token, ShaderPropertyType& outType)
            {
                if (token == "color") {
                    outType = ShaderPropertyType::Color;
                    return true;
                }
                if (token == "float") {
                    outType = ShaderPropertyType::Float;
                    return true;
                }
                if (token == "vec2" || token == "vector2") {
                    outType = ShaderPropertyType::Vector2;
                    return true;
                }
                if (token == "vec3" || token == "vector3") {
                    outType = ShaderPropertyType::Vector3;
                    return true;
                }
                if (token == "vec4" || token == "vector4") {
                    outType = ShaderPropertyType::Vector4;
                    return true;
                }
                if (token == "texture") {
                    outType = ShaderPropertyType::Texture;
                    return true;
                }
                return false;
            }

            bool TryParsePropertyDefinition(const std::string& value, ShaderPropertyDefinition& outDefinition)
            {
                const std::vector<std::string> parts = Split(value, ';');
                if (parts.size() < 2) {
                    return false;
                }

                ShaderPropertyDefinition definition;
                definition.uniformName = parts[0];
                definition.displayName = parts[0];

                if (!TryParsePropertyType(parts[1], definition.type)) {
                    return false;
                }

                if (parts.size() >= 3 && !parts[2].empty()) {
                    const std::vector<std::string> components = Split(parts[2], ',');
                    if (!components.empty()) {
                        definition.defaultValue.x = std::stof(components[0]);
                    }
                    if (components.size() > 1) {
                        definition.defaultValue.y = std::stof(components[1]);
                    }
                    if (components.size() > 2) {
                        definition.defaultValue.z = std::stof(components[2]);
                    }
                    if (components.size() > 3) {
                        definition.defaultValue.w = std::stof(components[3]);
                    }
                }

                if (definition.type == ShaderPropertyType::Float && parts.size() >= 5) {
                    definition.minValue = std::stof(parts[3]);
                    definition.maxValue = std::stof(parts[4]);
                    definition.hasRange = true;
                    if (parts.size() >= 6 && !parts[5].empty()) {
                        definition.displayName = parts[5];
                    }
                } else if (parts.size() >= 4 && !parts[3].empty()) {
                    if (definition.type == ShaderPropertyType::Float) {
                        definition.displayName = parts[3];
                    } else if (definition.type != ShaderPropertyType::Texture) {
                        definition.displayName = parts[3];
                    }
                }

                if (definition.displayName == definition.uniformName) {
                    if (definition.uniformName == "uColor") {
                        definition.displayName = "Color";
                    } else if (definition.uniformName == "uTexture") {
                        definition.displayName = "Texture";
                    }
                }
                outDefinition = definition;
                return !definition.uniformName.empty();
            }

            bool ReadTemplateFile(const std::string& path, std::string& outContents)
            {
                std::ifstream file(path);
                if (!file.is_open()) {
                    return false;
                }

                std::ostringstream buffer;
                buffer << file.rdbuf();
                outContents = buffer.str();
                return !outContents.empty();
            }

            bool WriteTextFile(const std::filesystem::path& path, const std::string& contents)
            {
                std::error_code ec;
                std::filesystem::create_directories(path.parent_path(), ec);

                std::ofstream file(path);
                if (!file.is_open()) {
                    return false;
                }

                file << contents;
                return true;
            }
        }

        bool ShaderAsset::ParseFile(const std::string& assetPath, ShaderAssetData& outData)
        {
            outData = {};

            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();
            const std::string resolvedPath = resources.ResolvePathForRead(assetPath);
            if (resolvedPath.empty()) {
                return false;
            }

            std::ifstream file(resolvedPath);
            if (!file.is_open()) {
                return false;
            }

            std::string line;
            while (std::getline(file, line)) {
                if (line.empty() || line[0] == '#') {
                    continue;
                }

                const size_t separator = line.find('=');
                if (separator == std::string::npos) {
                    continue;
                }

                std::string key = line.substr(0, separator);
                std::string value = line.substr(separator + 1);
                StripUtf8Bom(key);
                Trim(key);
                Trim(value);

                if (key == "vertex") {
                    outData.vertexPath = value;
                } else if (key == "fragment") {
                    outData.fragmentPath = value;
                } else if (key == "property") {
                    ShaderPropertyDefinition definition;
                    if (TryParsePropertyDefinition(value, definition)) {
                        outData.properties.push_back(definition);
                    }
                }
            }

            if (outData.properties.empty()) {
                outData.properties = {
                    []() {
                        ShaderPropertyDefinition colorProperty;
                        colorProperty.uniformName = "uColor";
                        colorProperty.displayName = "Color";
                        colorProperty.type = ShaderPropertyType::Color;
                        return colorProperty;
                    }(),
                    []() {
                        ShaderPropertyDefinition textureProperty;
                        textureProperty.uniformName = "uTexture";
                        textureProperty.displayName = "Texture";
                        textureProperty.type = ShaderPropertyType::Texture;
                        return textureProperty;
                    }()
                };
            }

            return !outData.vertexPath.empty() && !outData.fragmentPath.empty();
        }

        bool ShaderAsset::SaveFile(const std::string& assetPath, const ShaderAssetData& data)
        {
            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();
            const std::string resolvedPath = resources.ResolvePathForRead(assetPath);
            if (resolvedPath.empty()) {
                return false;
            }

            std::ofstream file(resolvedPath);
            if (!file.is_open()) {
                return false;
            }

            file << "vertex=" << data.vertexPath << "\n";
            file << "fragment=" << data.fragmentPath << "\n";
            for (const ShaderPropertyDefinition& property : data.properties) {
                file << "property=" << property.uniformName << ';' << FormatPropertyType(property.type) << ';'
                    << property.defaultValue.x << ',' << property.defaultValue.y << ','
                    << property.defaultValue.z << ',' << property.defaultValue.w;
                if (property.type == ShaderPropertyType::Float && property.hasRange) {
                    file << ';' << property.minValue << ';' << property.maxValue;
                    if (!property.displayName.empty()
                        && property.displayName != property.uniformName
                        && !(property.uniformName == "uColor" && property.displayName == "Color")
                        && !(property.uniformName == "uTexture" && property.displayName == "Texture")) {
                        file << ';' << property.displayName;
                    }
                } else if (!property.displayName.empty()
                    && property.displayName != property.uniformName
                    && !(property.uniformName == "uColor" && property.displayName == "Color")
                    && !(property.uniformName == "uTexture" && property.displayName == "Texture")) {
                    file << ';' << property.displayName;
                }
                file << "\n";
            }
            return true;
        }

        bool ShaderAsset::CreateTemplate(const std::filesystem::path& assetPath,
                                         const std::filesystem::path& assetRoot)
        {
            if (assetPath.empty() || assetPath.extension() != ".shader") {
                return false;
            }

            std::error_code ec;
            std::filesystem::create_directories(assetPath.parent_path(), ec);

            const std::string shaderStem = assetPath.stem().string();
            const std::filesystem::path vertexPath = assetPath.parent_path() / (shaderStem + ".vert");
            const std::filesystem::path fragmentPath = assetPath.parent_path() / (shaderStem + ".frag");

            std::filesystem::path relativeVertex =
                std::filesystem::relative(vertexPath, assetRoot, ec);
            std::filesystem::path relativeFragment =
                std::filesystem::relative(fragmentPath, assetRoot, ec);
            if (ec) {
                relativeVertex = std::filesystem::path("Assets") / "Shaders" / (shaderStem + ".vert");
                relativeFragment = std::filesystem::path("Assets") / "Shaders" / (shaderStem + ".frag");
            }

            std::string vertexTemplate;
            std::string fragmentTemplate;
            if (!ReadTemplateFile("Default/Shaders/basic.vert", vertexTemplate)) {
                vertexTemplate =
                    "#version 430 core\n"
                    "layout(location = 0) in vec3 aPosition;\n"
                    "layout(location = 1) in vec3 aNormal;\n"
                    "layout(location = 2) in vec2 aTexCoords;\n"
                    "out vec3 vNormal;\n"
                    "out vec3 vFragPos;\n"
                    "uniform mat4 uModel;\n"
                    "layout(std140, binding = 1) uniform CameraData {\n"
                    "    mat4 view;\n"
                    "    mat4 projection;\n"
                    "    vec3 viewPos;\n"
                    "};\n"
                    "void main() {\n"
                    "    vec4 worldPos = uModel * vec4(aPosition, 1.0);\n"
                    "    gl_Position = projection * view * worldPos;\n"
                    "    vFragPos = worldPos.xyz;\n"
                    "    vNormal = mat3(transpose(inverse(uModel))) * aNormal;\n"
                    "}\n";
            }

            if (!ReadTemplateFile("Default/Shaders/basic.frag", fragmentTemplate)) {
                fragmentTemplate =
                    "#version 430 core\n"
                    "in vec3 vNormal;\n"
                    "in vec3 vFragPos;\n"
                    "uniform vec4 uColor;\n"
                    "out vec4 FragColor;\n"
                    "void main() {\n"
                    "    FragColor = vec4(uColor.rgb, uColor.a);\n"
                    "}\n";
            }

            if (!WriteTextFile(vertexPath, vertexTemplate) ||
                !WriteTextFile(fragmentPath, fragmentTemplate)) {
                return false;
            }

            ShaderAssetData data;
            data.vertexPath = relativeVertex.generic_string();
            data.fragmentPath = relativeFragment.generic_string();
            for (char& character : data.vertexPath) {
                if (character == '\\') {
                    character = '/';
                }
            }
            for (char& character : data.fragmentPath) {
                if (character == '\\') {
                    character = '/';
                }
            }

            ShaderPropertyDefinition colorProperty;
            colorProperty.uniformName = "uColor";
            colorProperty.displayName = "Color";
            colorProperty.type = ShaderPropertyType::Color;
            data.properties.push_back(colorProperty);

            ShaderPropertyDefinition textureProperty;
            textureProperty.uniformName = "uTexture";
            textureProperty.displayName = "Texture";
            textureProperty.type = ShaderPropertyType::Texture;
            data.properties.push_back(textureProperty);

            std::filesystem::path relativeAsset =
                std::filesystem::relative(assetPath, assetRoot, ec);
            if (ec) {
                relativeAsset = std::filesystem::path("Assets") / "Shaders" / assetPath.filename();
            }

            const std::string assetRef = relativeAsset.generic_string();
            return SaveFile(assetRef, data);
        }

    }
}
