#include "Material.h"
namespace RTBEngine {
    namespace Rendering {

		Material::Material(Shader* shader) : 
            shader(shader), texture(nullptr), color(Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f)), shininess(32.0f)
        {
            
        }

        Material::~Material()
        {
            
        }

        void Material::Bind()
        {
            if (shader) {
                shader->Bind();
                shader->SetVector4("uColor", color);
                shader->SetFloat("uShininess", shininess);
                if (texture) {
                    shader->SetInt("uTexture", 0);
                }
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
            if (shader) {
                shader->Unbind();
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

    }
}