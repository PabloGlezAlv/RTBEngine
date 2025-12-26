#include "MeshRenderer.h"
#include "GameObject.h"
#include "../Rendering/Lighting/Light.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "../Animation/Animator.h"

namespace RTBEngine {
    namespace ECS {

        MeshRenderer::MeshRenderer()
            : Component()
        {
            material = std::make_unique<Rendering::Material>(nullptr);
        }

        MeshRenderer::~MeshRenderer()
        {

        }

        void MeshRenderer::SetMesh(Rendering::Mesh* mesh)
        {
            meshes.clear();
            if (mesh) {
                meshes.push_back(mesh);
            }
        }

        void MeshRenderer::SetMeshes(const std::vector<Rendering::Mesh*>& newMeshes)
        {
            meshes = newMeshes;
        }

        void MeshRenderer::SetTexture(Rendering::Texture* tex) {
            if (material) {
                material->SetTexture(tex);
            }
        }

        void MeshRenderer::SetShader(Rendering::Shader* shader) {
            if (material) {
                material->SetShader(shader);
            }
        }


        void MeshRenderer::Render(Rendering::Camera* camera, const std::vector<Rendering::Light*>& lights)
        {
            if (!isEnabled || meshes.empty() || !material || !owner) {
                return;
            }

            material->Bind();

            if (material->GetShader()) {
                Math::Matrix4 model = owner->GetTransform().GetModelMatrix();
                material->GetShader()->SetMatrix4("uModel", model);
                material->GetShader()->SetMatrix4("uView", camera->GetViewMatrix());
                material->GetShader()->SetMatrix4("uProjection", camera->GetProjectionMatrix());

                material->GetShader()->SetVector3("uViewPos", camera->GetPosition());

                // Skeletal animation
                Animation::Animator* animator = owner->GetComponent<Animation::Animator>();
                if (animator && animator->HasBones()) {
                    material->GetShader()->SetBool("uHasAnimation", true);
                    const std::vector<Math::Matrix4>& boneTransforms = animator->GetBoneTransforms();
                    for (size_t i = 0; i < boneTransforms.size() && i < 100; i++) {
                        material->GetShader()->SetMatrix4("uBoneTransforms[" + std::to_string(i) + "]", boneTransforms[i]);
                    }
                }
                else {
                    material->GetShader()->SetBool("uHasAnimation", false);
                }

                // Lighting
                if (!lights.empty()) {
                    lights[0]->ApplyToShader(material->GetShader());
                }
                else {
                    material->GetShader()->SetVector3("uLightDir", Math::Vector3(0.0f, -1.0f, 0.0f));
                    material->GetShader()->SetVector3("uLightColor", Math::Vector3(1.0f, 1.0f, 1.0f));
                }
            }

            // Draw all meshes
            for (Rendering::Mesh* mesh : meshes) {
                if (mesh) {
                    mesh->Draw();
                }
            }

            material->Unbind();
        }

    }
}
