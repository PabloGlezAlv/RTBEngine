#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector4.h"
#include <string>
#include <unordered_map>
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

        // Parsed shader-property overrides; rebuild when shaderRef or overrideBlob changes.
        class RTB_API ShaderPropertyOverrideCache {
        public:
            void Rebuild(const std::string& shaderRef, const std::string& overrideBlob);
            bool Matches(const std::string& shaderRef, const std::string& overrideBlob) const;
            void ApplyExtraUniforms(class Shader* shader,
                                      const std::string& shaderRef,
                                      const Math::Vector4& colorRef) const;

        private:
            struct CachedEntry {
                ShaderPropertyType type = ShaderPropertyType::Float;
                bool hasOverride = false;
                Math::Vector4 vectorValue{};
                float floatValue = 0.0f;
            };

            std::string cachedShaderRef;
            std::string cachedOverrideBlob;
            std::unordered_map<std::string, CachedEntry> entries;
        };

        class RTB_API ShaderProperties {
        public:
            static const std::vector<ShaderPropertyDefinition>& GetDefinitions(const std::string& shaderRef);
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
                                           const ShaderPropertyOverrideCache& overrideCache,
                                           const Math::Vector4& colorRef);
            static void ApplyEngineUniforms(class Shader* shader);
        };

    }
}
