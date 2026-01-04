#include "MeshRenderer.h"
#include "GameObject.h"
#include "../Rendering/Lighting/Light.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "../Animation/Animator.h"
#include "../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace ECS {

        using ThisClass = MeshRenderer;
        RTB_REGISTER_COMPONENT(MeshRenderer)
        RTB_END_REGISTER(MeshRenderer)

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

        void MeshRenderer::SetMeshMaterials(const std::vector<Rendering::Material*>& mats)
        {
            meshMaterials = mats;
        }

        Rendering::Material* MeshRenderer::GetMeshMaterial(size_t meshIndex) const
        {
            if (meshIndex < meshMaterials.size() && meshMaterials[meshIndex]) {
                return meshMaterials[meshIndex];
            }
            return material.get();
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
            if (!isEnabled || meshes.empty() || !owner) {
                return;
            }

            // Get common data
            Math::Matrix4 modelMatrix = owner->GetWorldMatrix();
            Animation::Animator* animator = owner->GetComponent<Animation::Animator>();

            // Draw each mesh with its material
            for (size_t i = 0; i < meshes.size(); i++) {
                Rendering::Mesh* mesh = meshes[i];
                if (!mesh) continue;

                // Get material for this mesh
                Rendering::Material* mat = GetMeshMaterial(i);
                if (!mat) continue;

                mat->Bind();

                Rendering::Shader* shader = mat->GetShader();
                if (shader) {
                    shader->SetMatrix4("uModel", modelMatrix);
                    shader->SetMatrix4("uView", camera->GetViewMatrix());
                    shader->SetMatrix4("uProjection", camera->GetProjectionMatrix());
                    shader->SetVector3("uViewPos", camera->GetPosition());

                    // Skeletal animation
                    if (animator && animator->HasBones()) {
                        shader->SetBool("uHasAnimation", true);
                        const std::vector<Math::Matrix4>& boneTransforms = animator->GetBoneTransforms();
                        for (size_t j = 0; j < boneTransforms.size() && j < 100; j++) {
                            shader->SetMatrix4("uBoneTransforms[" + std::to_string(j) + "]", boneTransforms[j]);
                        }
                    }
                    else {
                        shader->SetBool("uHasAnimation", false);
                    }

                    // Lighting
                    if (!lights.empty()) {
                        lights[0]->ApplyToShader(shader);
                    }
                    else {
                        shader->SetVector3("uLightDir", Math::Vector3(0.0f, -1.0f, 0.0f));
                        shader->SetVector3("uLightColor", Math::Vector3(1.0f, 1.0f, 1.0f));
                    }
                }

                mesh->Draw();
                mat->Unbind();
            }
        }

    }
}
