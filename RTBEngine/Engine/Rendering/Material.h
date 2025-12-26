#pragma once
#include "Shader.h"
#include "Texture.h"
#include "../Math/Math.h"

namespace RTBEngine {
    namespace Rendering {

        class Material {
        public:
            Material(Shader* shader);
            ~Material();

            Material(const Material&) = delete;
            Material& operator=(const Material&) = delete;

            void Bind();
            void Unbind();

            void SetShader(Shader* shader);
            void SetTexture(Texture* texture);
            void SetColor(const Math::Vector4& color);
            void SetShininess(float shininess);
            void SetDiffuseColor(const Math::Vector3& color);

            Shader* GetShader() const { return shader; }
            Texture* GetTexture() const { return texture; }
            const Math::Vector4& GetColor() const { return color; }
            float GetShininess() const { return shininess; }
            const Math::Vector3& GetDiffuseColor() const { return diffuseColor; }

        private:
            Shader* shader;
            Texture* texture;
            Math::Vector4 color;
            Math::Vector3 diffuseColor;
            float shininess;
        };

    }
}