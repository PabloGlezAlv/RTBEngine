#include "RayTracingScene.h"
#include "../Mesh.h"
#include "../RHI/RenderDevice.h"
#include "../../Scene/Scene.h"
#include "../../Scene/MeshRenderer.h"
#include "../../Scene/GameObject.h"
#include "../../Animation/Animator.h"
#include "../../Core/Logger.h"

namespace RTBEngine {
    namespace Rendering {
        namespace GI {

            RayTracingScene::~RayTracingScene()
            {
                Clear();
            }

            void RayTracingScene::Clear()
            {
                if (!RHI::RenderDevice::HasDevice()) {
                    blasEntries.clear();
                    instances.clear();
                    tlasHandle = nullptr;
                    valid = false;
                    return;
                }

                auto& device = RHI::RenderDevice::Get();
                for (BlasEntry& entry : blasEntries) {
                    if (entry.deviceVertexBuffer != RHI::kInvalidGpuId) {
                        device.DestroyBuffer(entry.deviceVertexBuffer);
                    }
                    if (entry.deviceIndexBuffer != RHI::kInvalidGpuId) {
                        device.DestroyBuffer(entry.deviceIndexBuffer);
                    }
                }
                blasEntries.clear();
                instances.clear();
                tlasHandle = nullptr;
                valid = false;
            }

            void RayTracingScene::Rebuild(Scene::Scene* scene)
            {
                Clear();
                if (!scene || !RHI::RenderDevice::HasDevice()) {
                    return;
                }

                const auto& caps = RHI::RenderDevice::Get().GetGiCapabilities();
                if (!caps.rayQuery) {
                    return;
                }

                // Collect static mesh instances that contribute to GI (skip skinned meshes in v1).
                for (Scene::MeshRenderer* renderer : scene->GetCachedMeshRenderers()) {
                    if (!renderer || !renderer->IsEnabled()) continue;
                    Scene::GameObject* owner = renderer->GetOwner();
                    if (!owner || !owner->IsActiveInHierarchy()) continue;
                    if (!owner->HasStaticFlag(Scene::StaticFlags::ContributeGI)) continue;

                    Mesh* mesh = renderer->GetMesh();
                    if (!mesh || mesh->GetIndexCount() == 0) continue;

                    Animation::Animator* animator = renderer->GetActiveAnimator();
                    const bool skinned = animator && animator->ShouldSkinMesh();
                    if (skinned) continue;

                    RayTracingMeshInstance inst{};
                    inst.mesh = mesh;
                    inst.worldMatrix = owner->GetWorldMatrix();
                    inst.skinned = false;
                    instances.push_back(inst);
                }

                valid = !instances.empty();
            }

        }
    }
}
