#include "MeshRenderer.h"
#include "GameObject.h"

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

        void MeshRenderer::Render(Rendering::Camera* camera)
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
            }

            mesh->Draw();
            material->Unbind();
        }

    }
}