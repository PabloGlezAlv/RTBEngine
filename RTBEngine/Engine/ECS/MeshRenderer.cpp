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
        RTB_END_REGISTER(MeshRenderer)

        MeshRenderer::MeshRenderer()
            : Component()
        {
            material = std::make_unique<Rendering::Material>(nullptr);
            // Default material values
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
            // Ensure material has a shader
            if (material && !material->GetShader()) {
                Rendering::Shader* shader = Core::ResourceManager::GetInstance().GetShader("basic");
                if (shader) material->SetShader(shader);
            }

            // Mesh
            bool meshChanged = (!meshes.empty()) ? (meshes[0] != meshRef) : (meshRef != nullptr);

            if (meshChanged) {
                RTB_INFO(std::string("[SYNC] GO='") + (owner ? owner->GetName() : "null") +
                    "' meshRef=" + (meshRef ? "valid" : "null") +
                    " meshes.size()=" + std::to_string(meshes.size()) +
                    " meshChanged=true");
                if (meshRef) {
                    auto& rm = Core::ResourceManager::GetInstance();
                    std::string path = rm.GetMeshPath(meshRef);
                    RTB_INFO(std::string("[SYNC] Mesh changed. meshRef ptr=") + std::to_string(reinterpret_cast<size_t>(meshRef)) +
                        " GetMeshPath='" + path + "'");
                    if (!path.empty()) {
                        const auto& allMeshes = rm.GetModelMeshes(path);
                        if (allMeshes.size() > 1) {
                            RTB_INFO(std::string("[SYNC] Multi-mesh model '") + path + "' allMeshes.size()=" + std::to_string(allMeshes.size()) + ". Calling SetMeshes.");
                            SetMeshes(allMeshes);
                        } else {
                            RTB_INFO(std::string("[SYNC] Single-mesh path '") + path + "'. Calling SetMesh.");
                            SetMesh(meshRef);
                        }
                    } else {
                        RTB_WARN(std::string("[SYNC WARNING] Path empty for meshRef ptr=") + std::to_string(reinterpret_cast<size_t>(meshRef)) + ". Calling SetMesh directly.");
                        SetMesh(meshRef);
                    }
                } else {
                    meshes.clear();
                }
            }

            // Material
            if (material) {
                if (material->GetTexture() != textureRef) {
                    material->SetTexture(textureRef);
                }
                material->SetColor(colorRef);
            }
        }

        void MeshRenderer::SetMesh(Rendering::Mesh* mesh)
        {
            meshes.clear();
            if (mesh) {
                meshes.push_back(mesh);
            }
            meshRef = mesh; // Keep synced
        }

        void MeshRenderer::SetMeshes(const std::vector<Rendering::Mesh*>& newMeshes)
        {
            meshes = newMeshes;
            if (!meshes.empty()) {
                meshRef = meshes[0];
            } else {
                meshRef = nullptr;
            }
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
                textureRef = tex; // Keep synced
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
            if (meshes.empty()) {
                RTB_WARN(std::string("[RENDER WARNING] GO='") + owner->GetName() + "' has no meshes to render (meshRef=" + (meshRef ? "valid" : "null") + ")");
                return;
            }

            // Get common data
            Math::Matrix4 modelMatrix = owner->GetWorldMatrix();
            Animation::Animator* animator = owner->GetComponent<Animation::Animator>();

            // Draw each mesh with its material
            for (size_t i = 0; i < meshes.size(); i++) {
                Rendering::Mesh* mesh = meshes[i];
                if (!mesh) {
                    RTB_WARN(std::string("[RENDER] GO='") + owner->GetName() + "' mesh[" + std::to_string(i) + "] is null");
                    continue;
                }

                // Get material for this mesh
                Rendering::Material* mat = GetMeshMaterial(i);
                if (!mat) {
                    RTB_WARN(std::string("[RENDER] GO='") + owner->GetName() + "' material[" + std::to_string(i) + "] is null");
                    continue;
                }

                mat->Bind();

                Rendering::Shader* shader = mat->GetShader();
                if (!shader) {
                    RTB_WARN(std::string("[RENDER] GO='") + owner->GetName() + "' shader is null for mesh[" + std::to_string(i) + "]");
                }
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
                            if (pointLightCount < 8) { // MAX_POINT_LIGHTS in shader is 8
                                auto* pl = static_cast<Rendering::PointLight*>(light);
                                pl->ApplyToShader(shader, pointLightCount++);
                            }
                        }
                        else if (type == Rendering::LightType::Spot) {
                            if (spotLightCount < 8) { // MAX_SPOT_LIGHTS in shader is 8
                                auto* sl = static_cast<Rendering::SpotLight*>(light);
                                sl->ApplyToShader(shader, spotLightCount++);
                            }
                        }
                    }

                    shader->SetInt("numPointLights", pointLightCount);
                    shader->SetInt("numSpotLights", spotLightCount);

                    if (!directionalLightSet) {
                        // Reset defaults if no directional light
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
}
