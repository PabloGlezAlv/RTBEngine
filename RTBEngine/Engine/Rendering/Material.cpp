#include "Material.h"
namespace RTBEngine {
    namespace Rendering {

		Material::Material(Shader* shader) :
            shader(shader), texture(nullptr),
            color(Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f)),
            diffuseColor(Math::Vector3(1.0f, 1.0f, 1.0f)),
            shininess(32.0f)
        {

        }

        Material::~Material()
        {
            
        }

        void Material::ApplyProperties()
        {
            if (!shader) {
                return;
            }

            shader->SetVector4("uColor", color);
            shader->SetVector3("uDiffuseColor", diffuseColor);
            shader->SetFloat("uShininess", shininess);
            shader->SetBool("uHasTexture", texture != nullptr);
            if (texture) {
                shader->SetInt("uTexture", 0);
            }
        }

        void Material::Bind()
        {
            if (shader) {
                shader->Bind();
                ApplyProperties();
            }
            if (texture) {
                texture->Bind(0);
            }
        }

        void Material::Unbind()
        {
            if (texture) {
                texture->Unbind();
            }
        }

        void Material::SetShader(Shader* shader)
        {
			this->shader = shader;
        }

        void Material::SetTexture(Texture* texture)
        {
			this->texture = texture;
        }

        void Material::SetColor(const Math::Vector4& color)
        {
            this->color = color;
        }

        void Material::SetShininess(float shininess)
        {
			this->shininess = shininess;
        }

        void Material::SetDiffuseColor(const Math::Vector3& color)
        {
            this->diffuseColor = color;
        }

    }
}