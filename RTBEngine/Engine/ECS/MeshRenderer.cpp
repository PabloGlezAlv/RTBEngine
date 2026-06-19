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

        namespace {
            Animation::Animator* FindAnimatorInAncestors(GameObject* start)
            {
                for (GameObject* current = start; current; current = current->GetParent()) {
                    if (auto* animator = current->GetComponent<Animation::Animator>()) {
                        return animator;
                    }
                }
                return nullptr;
            }
        }

        uint32_t MeshRenderer::drawCallCount = 0;
        uint32_t MeshRenderer::triangleCount = 0;
        uint32_t MeshRenderer::culledObjectCount = 0;

        void MeshRenderer::ResetRenderStats() {
            drawCallCount = 0;
            triangleCount = 0;
            culledObjectCount = 0;
        }

        using ThisClass = MeshRenderer;
        RTB_REGISTER_COMPONENT(MeshRenderer)
            RTB_PROPERTY_MESH(meshRef)
            RTB_PROPERTY_TEXTURE(textureRef)
            RTB_PROPERTY_COLOR(colorRef)
            RTB_PROPERTY(meshIndex)
            RTB_PROPERTY(multiMesh)
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
            // Multi-mesh mode does not use reflected proxies for mesh/texture sync
            if (multiMesh) return;

            if (material && !material->GetShader()) {
                Rendering::Shader* shader = Core::ResourceManager::GetInstance().GetShader("basic");
                if (shader) material->SetShader(shader);
            }

            // Sync mesh from reflected proxy
            if (mesh != meshRef) {
                mesh = meshRef;
            }

            // Sync material properties from reflected proxies.
            // Never clear an FBX-assigned texture when textureRef is still null.
            if (material) {
                if (textureRef) {
                    if (material->GetTexture() != textureRef) {
                        material->SetTexture(textureRef);
                    }
                }
                else if (material->GetTexture()) {
                    textureRef = material->GetTexture();
                }
                material->SetColor(colorRef);
            }
        }

        void MeshRenderer::SetMesh(Rendering::Mesh* newMesh)
        {
            mesh = newMesh;
            meshRef = newMesh;
        }

        void MeshRenderer::SetMeshes(const std::vector<Rendering::Mesh*>& meshList)
        {
            meshes = meshList;
            multiMesh = meshList.size() > 1;

            // Create a default material for each mesh
            meshMaterials.clear();
            Rendering::Shader* basicShader = Core::ResourceManager::GetInstance().GetShader("basic");
            for (size_t i = 0; i < meshList.size(); i++) {
                auto mat = std::make_unique<Rendering::Material>(basicShader);
                meshMaterials.push_back(std::move(mat));
            }

            // Keep single-mesh state in sync for backward compat
            if (!meshList.empty()) {
                mesh = meshList[0];
                meshRef = meshList[0];
            }
        }

        void MeshRenderer::SetMaterialForMesh(int index, Rendering::Material* mat)
        {
            if (index < 0 || index >= static_cast<int>(meshMaterials.size())) return;
            if (!mat) return;

            meshMaterials[index]->SetShader(mat->GetShader());
            meshMaterials[index]->SetTexture(mat->GetTexture());
            meshMaterials[index]->SetColor(mat->GetColor());
            meshMaterials[index]->SetShininess(mat->GetShininess());
            meshMaterials[index]->SetDiffuseColor(mat->GetDiffuseColor());
        }

        Rendering::Material* MeshRenderer::GetMaterialForMesh(int index) const
        {
            if (index < 0 || index >= static_cast<int>(meshMaterials.size())) return nullptr;
            return meshMaterials[index].get();
        }

        void MeshRenderer::GetCombinedAABB(Math::Vector3& outMin, Math::Vector3& outMax) const {
            if (multiMesh) {
                if (meshes.empty()) {
                    outMin = outMax = Math::Vector3::Zero();
                    return;
                }
                outMin = meshes[0]->GetAABBMin();
                outMax = meshes[0]->GetAABBMax();
                for (size_t i = 1; i < meshes.size(); i++) {
                    if (!meshes[i]) continue;
                    Math::Vector3 mn = meshes[i]->GetAABBMin();
                    Math::Vector3 mx = meshes[i]->GetAABBMax();
                    outMin.x = std::min(outMin.x, mn.x);
                    outMin.y = std::min(outMin.y, mn.y);
                    outMin.z = std::min(outMin.z, mn.z);
                    outMax.x = std::max(outMax.x, mx.x);
                    outMax.y = std::max(outMax.y, mx.y);
                    outMax.z = std::max(outMax.z, mx.z);
                }
            }
            else {
                if (!mesh) {
                    outMin = outMax = Math::Vector3::Zero();
                    return;
                }
                outMin = mesh->GetAABBMin();
                outMax = mesh->GetAABBMax();
            }
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
            if (!isEnabled || !owner || !owner->IsActiveInHierarchy()) {
                return;
            }

            if (multiMesh) {
                RenderMultiMesh(camera, lights);
                return;
            }

            //Single-mesh path (unchanged)
            if (!mesh) {
                RTB_WARN(std::string("[RENDER] GO='") + owner->GetName() + "' has no mesh");
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

                // Skeletal animation: walk up hierarchy (KayKit/multi-mesh FBX children)
                Animation::Animator* animator = FindAnimatorInAncestors(owner);

                if (animator && animator->ShouldSkinMesh()) {
                    shader->SetBool("uHasAnimation", true);
                    shader->SetBoneTransforms(animator->GetBoneTransforms());
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

        void MeshRenderer::RenderMultiMesh(Rendering::Camera* camera, const std::vector<Rendering::Light*>& lights)
        {
            if (meshes.empty()) {
                RTB_WARN(std::string("[RENDER] GO='") + owner->GetName() + "' multi-mesh has no meshes");
                return;
            }

            // Find first valid shader from materials
            Rendering::Shader* shader = nullptr;
            for (auto& mat : meshMaterials) {
                if (mat && mat->GetShader()) {
                    shader = mat->GetShader();
                    break;
                }
            }
            if (!shader) {
                RTB_WARN(std::string("[RENDER] GO='") + owner->GetName() + "' multi-mesh has no shader");
                return;
            }

            // Setup shared uniforms once
            Math::Matrix4 modelMatrix = owner->GetWorldMatrix();
            shader->Bind();
            shader->SetMatrix4("uModel", modelMatrix);
            shader->SetMatrix4("uView", camera->GetViewMatrix());
            shader->SetMatrix4("uProjection", camera->GetProjectionMatrix());
            shader->SetVector3("uViewPos", camera->GetPosition());

            Animation::Animator* animator = FindAnimatorInAncestors(owner);

            if (animator && animator->ShouldSkinMesh()) {
                shader->SetBool("uHasAnimation", true);
                shader->SetBoneTransforms(animator->GetBoneTransforms());
            }
            else {
                shader->SetBool("uHasAnimation", false);
            }

            // Lighting (set once, shared across all sub-meshes)
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

            // Draw each sub-mesh with its own material
            for (size_t i = 0; i < meshes.size(); i++) {
                if (!meshes[i]) continue;

                if (i < meshMaterials.size() && meshMaterials[i]) {
                    meshMaterials[i]->Bind();
                }

                meshes[i]->Draw();
                drawCallCount++;
                triangleCount += meshes[i]->GetIndexCount() / 3;

                if (i < meshMaterials.size() && meshMaterials[i]) {
                    meshMaterials[i]->Unbind();
                }
            }
        }

    }
}
