#include "MeshRenderer.h"
#include "GameObject.h"
#include "../Rendering/Lighting/Light.h"
#include "../Rendering/Lighting/LightingUBO.h"
#include "../Rendering/CameraUBO.h"
#include "../Animation/Animator.h"
#include "../Reflection/PropertyMacros.h"
#include "../Core/ResourceManager.h"
#include "../Core/Logger.h"
#include <algorithm>

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

        void MeshRenderer::SetOcclusionFadeAlpha(float alpha)
        {
            occlusionFadeAlpha = std::clamp(alpha, 0.0f, 1.0f);
        }

        Animation::Animator* MeshRenderer::GetActiveAnimator()
        {
            const uint32_t hierarchyVersion = GameObject::GetHierarchyVersion();
            if (!animatorCacheValid || hierarchyVersion != cachedAnimatorHierarchyVersion) {
                cachedAnimator = FindAnimatorInAncestors(owner);
                cachedAnimatorHierarchyVersion = hierarchyVersion;
                animatorCacheValid = true;
            }
            return cachedAnimator;
        }

        void MeshRenderer::RenderDraw(Rendering::Mesh* drawMesh, Rendering::Material* drawMaterial)
        {
            if (!isEnabled || !owner || !owner->IsActiveInHierarchy() || !drawMesh || !drawMaterial) {
                return;
            }

            Rendering::Shader* shader = drawMaterial->GetShader();
            if (!shader) {
                RTB_WARN(std::string("[RENDER] GO='") + owner->GetName() + "' shader is null");
                return;
            }

            shader->SetMatrix4("uModel", owner->GetWorldMatrix());

            Animation::Animator* animator = GetActiveAnimator();
            if (animator && animator->ShouldSkinMesh()) {
                shader->SetBool("uHasAnimation", true);
                animator->BindBoneMatrices();
            }
            else {
                shader->SetBool("uHasAnimation", false);
            }

            drawMesh->Draw();
            drawCallCount++;
            triangleCount += drawMesh->GetIndexCount() / 3;
        }

        void MeshRenderer::Render()
        {
            if (!isEnabled || !owner || !owner->IsActiveInHierarchy()) {
                return;
            }

            if (multiMesh) {
                RenderMultiMesh();
                return;
            }

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
            Rendering::LightingUBO::GetInstance().Bind();
            Rendering::CameraUBO::GetInstance().Bind();
            RenderDraw(mesh, mat);
            mat->Unbind();
        }

        void MeshRenderer::RenderMultiMesh()
        {
            if (meshes.empty()) {
                RTB_WARN(std::string("[RENDER] GO='") + owner->GetName() + "' multi-mesh has no meshes");
                return;
            }

            for (size_t i = 0; i < meshes.size(); i++) {
                if (!meshes[i]) {
                    continue;
                }

                Rendering::Material* subMaterial =
                    i < meshMaterials.size() ? meshMaterials[i].get() : nullptr;
                if (!subMaterial) {
                    continue;
                }

                subMaterial->Bind();
                Rendering::LightingUBO::GetInstance().Bind();
                Rendering::CameraUBO::GetInstance().Bind();
                RenderDraw(meshes[i], subMaterial);
                subMaterial->Unbind();
            }
        }

    }
}
