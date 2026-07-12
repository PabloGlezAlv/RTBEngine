#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector4.h"
#include <string>
#include <vector>

namespace RTBEngine {
    namespace Rendering {

        enum class ShaderPropertyType {
            Color,
            Float,
            Vector2,
            Vector3,
            Vector4,
            Texture
        };

        struct ShaderPropertyDefinition {
            std::string uniformName;
            std::string displayName;
            ShaderPropertyType type = ShaderPropertyType::Color;
            Math::Vector4 defaultValue = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            float minValue = 0.0f;
            float maxValue = 1.0f;
            bool hasRange = false;
        };

        class RTB_API ShaderProperties {
        public:
            static std::vector<ShaderPropertyDefinition> GetDefinitions(const std::string& shaderRef);
            static Math::Vector4 ResolveColorValue(
                const ShaderPropertyDefinition& definition,
                const Math::Vector4& colorRef,
                const std::string& overrideBlob);
            static float ResolveFloatValue(
                const ShaderPropertyDefinition& definition,
                const std::string& overrideBlob);
            static Math::Vector4 ResolveVectorValue(
                const ShaderPropertyDefinition& definition,
                const std::string& overrideBlob);
            static void SetColorOverride(std::string& overrideBlob,
                                         const std::string& uniformName,
                                         const Math::Vector4& value);
            static void SetFloatOverride(std::string& overrideBlob,
                                         const std::string& uniformName,
                                         float value);
            static void SetVectorOverride(std::string& overrideBlob,
                                          const std::string& uniformName,
                                          const Math::Vector4& value,
                                          int componentCount = 4);
            static void ApplyExtraUniforms(class Shader* shader,
                                           const std::string& shaderRef,
                                           const std::string& overrideBlob,
                                           const Math::Vector4& colorRef);
            static void ApplyEngineUniforms(class Shader* shader);
        };

    }
}
