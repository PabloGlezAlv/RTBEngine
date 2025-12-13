#include "MeshRenderer.h"
#include "GameObject.h"
#include "../Rendering/Lighting/Light.h"
#include "../Rendering/Lighting/DirectionalLight.h"

namespace RTBEngine {
    namespace ECS {

        MeshRenderer::MeshRenderer()
            : Component()
            , mesh(nullptr)
            , material(nullptr)
        {
        }

        MeshRenderer::~MeshRenderer()
        {
            // No posee los recursos, solo referencias
        }

        void MeshRenderer::SetMesh(Rendering::Mesh* mesh)
        {
            this->mesh = mesh;
        }

        void MeshRenderer::SetMaterial(Rendering::Material* material)
        {
            this->material = material;
        }

        void MeshRenderer::Render(Rendering::Camera* camera, const std::vector<Rendering::Light*>& lights)
        {
            if (!isEnabled || !mesh || !material || !owner) {
                return;
            }

            material->Bind();

            if (material->GetShader()) {
                Math::Matrix4 model = owner->GetTransform().GetModelMatrix();
                material->GetShader()->SetMatrix4("uModel", model);
                material->GetShader()->SetMatrix4("uView", camera->GetViewMatrix());
                material->GetShader()->SetMatrix4("uProjection", camera->GetProjectionMatrix());

                material->GetShader()->SetVector3("uViewPos", camera->GetPosition());

				//Lighting
                if (!lights.empty()) {
                    lights[0]->ApplyToShader(material->GetShader());
                }
                else {
                    material->GetShader()->SetVector3("uLightDir", Math::Vector3(0.0f, -1.0f, 0.0f));
                    material->GetShader()->SetVector3("uLightColor", Math::Vector3(1.0f, 1.0f, 1.0f));
                }
            }

            mesh->Draw();
            material->Unbind();
        }

    }
}