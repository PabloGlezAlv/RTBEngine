#include "MeshRenderer.h"
#include "GameObject.h"
#include "../Rendering/Lighting/Light.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "../Rendering/Lighting/PointLight.h"
#include "../Rendering/Lighting/SpotLight.h"
#include "../Animation/Animator.h"
#include "../Reflection/PropertyMacros.h"
#include "../Core/ResourceManager.h"
#include "../Core/Logger.h"

namespace RTBEngine {
    namespace ECS {

        uint32_t MeshRenderer::drawCallCount = 0;
        uint32_t MeshRenderer::triangleCount = 0;

        void MeshRenderer::ResetRenderStats() {
            drawCallCount = 0;
            triangleCount = 0;
        }

        using ThisClass = MeshRenderer;
        RTB_REGISTER_COMPONENT(MeshRenderer)
            RTB_PROPERTY_MESH(meshRef)
            RTB_PROPERTY_TEXTURE(textureRef)
            RTB_PROPERTY_COLOR(colorRef)
            RTB_PROPERTY(meshIndex)
        RTB_END_REGISTER(MeshRenderer)

        MeshRenderer::MeshRenderer()
            : Component()
        {
            material = std::make_unique<Rendering::Material>(nullptr);
            if (material) {
                colorRef = material->GetColor();
            }
        }

        MeshRenderer::~MeshRenderer()
        {
        }

        void MeshRenderer::OnAwake() {
            if (material && !material->GetShader()) {
                Rendering::Shader* shader = Core::ResourceManager::GetInstance().GetShader("basic");
                if (shader) material->SetShader(shader);
            }
            SyncProperties();
        }

        void MeshRenderer::OnUpdate(float deltaTime) {
            SyncProperties();
        }

        void MeshRenderer::OnValidate() {
            if (material && !material->GetShader()) {
                Rendering::Shader* shader = Core::ResourceManager::GetInstance().GetShader("basic");
                if (shader) material->SetShader(shader);
            }
            SyncProperties();
        }

        void MeshRenderer::SyncProperties() {
            if (material && !material->GetShader()) {
                Rendering::Shader* shader = Core::ResourceManager::GetInstance().GetShader("basic");
                if (shader) material->SetShader(shader);
            }

            // Sync mesh from reflected proxy
            if (mesh != meshRef) {
                mesh = meshRef;
            }

            // Sync material properties from reflected proxies
            if (material) {
                if (material->GetTexture() != textureRef) {
                    material->SetTexture(textureRef);
                }
                material->SetColor(colorRef);
            }
        }

        void MeshRenderer::SetMesh(Rendering::Mesh* newMesh)
        {
            mesh = newMesh;
            meshRef = newMesh;
        }

        void MeshRenderer::SetMaterial(Rendering::Material* mat)
        {
            if (!mat) return;
            // Copy fields from the provided material into our owned material
            material->SetShader(mat->GetShader());
            material->SetTexture(mat->GetTexture());
            material->SetColor(mat->GetColor());
            material->SetShininess(mat->GetShininess());
            material->SetDiffuseColor(mat->GetDiffuseColor());
            // Sync proxy so SyncProperties() doesn't clobber the texture on next frame
            textureRef = mat->GetTexture();
            colorRef = mat->GetColor();
        }

        void MeshRenderer::SetTexture(Rendering::Texture* tex) {
            if (material) {
                material->SetTexture(tex);
                textureRef = tex;
            }
        }

        void MeshRenderer::SetShader(Rendering::Shader* shader) {
            if (material) {
                material->SetShader(shader);
            }
        }

        void MeshRenderer::Render(Rendering::Camera* camera, const std::vector<Rendering::Light*>& lights)
        {
            if (!isEnabled || !owner) {
                return;
            }
            if (!mesh) {
                return;
            }

            Rendering::Material* mat = material.get();
            if (!mat) {
                RTB_WARN(std::string("[RENDER] GO='") + owner->GetName() + "' has no material");
                return;
            }

            mat->Bind();

            Rendering::Shader* shader = mat->GetShader();
            if (!shader) {
                RTB_WARN(std::string("[RENDER] GO='") + owner->GetName() + "' shader is null");
            }

            if (shader) {
                Math::Matrix4 modelMatrix = owner->GetWorldMatrix();
                shader->SetMatrix4("uModel", modelMatrix);
                shader->SetMatrix4("uView", camera->GetViewMatrix());
                shader->SetMatrix4("uProjection", camera->GetProjectionMatrix());
                shader->SetVector3("uViewPos", camera->GetPosition());

                // Skeletal animation: look on own GameObject first, then parent
                Animation::Animator* animator = owner->GetComponent<Animation::Animator>();
                if (!animator && owner->GetParent()) {
                    animator = owner->GetParent()->GetComponent<Animation::Animator>();
                }

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
                int pointLightCount = 0;
                int spotLightCount = 0;
                bool directionalLightSet = false;

                for (Rendering::Light* light : lights) {
                    if (!light) continue;

                    Rendering::LightType type = light->GetType();
                    if (type == Rendering::LightType::Directional) {
                        if (!directionalLightSet) {
                            light->ApplyToShader(shader);
                            directionalLightSet = true;
                        }
                    }
                    else if (type == Rendering::LightType::Point) {
                        if (pointLightCount < 8) {
                            auto* pl = static_cast<Rendering::PointLight*>(light);
                            pl->ApplyToShader(shader, pointLightCount++);
                        }
                    }
                    else if (type == Rendering::LightType::Spot) {
                        if (spotLightCount < 8) {
                            auto* sl = static_cast<Rendering::SpotLight*>(light);
                            sl->ApplyToShader(shader, spotLightCount++);
                        }
                    }
                }

                shader->SetInt("numPointLights", pointLightCount);
                shader->SetInt("numSpotLights", spotLightCount);

                if (!directionalLightSet) {
                    shader->SetVector3("dirLight.direction", Math::Vector3(0.0f, -1.0f, 0.0f));
                    shader->SetVector3("dirLight.color", Math::Vector3(0.0f, 0.0f, 0.0f));
                    shader->SetFloat("dirLight.intensity", 0.0f);
                }
            }

            mesh->Draw();
            drawCallCount++;
            triangleCount += mesh->GetIndexCount() / 3;
            mat->Unbind();
        }

    }
}
