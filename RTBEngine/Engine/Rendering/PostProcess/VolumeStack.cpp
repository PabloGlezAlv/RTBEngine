#include "VolumeStack.h"

#include "../../Scene/ComponentQuery.h"
#include "../../Scene/VolumeComponent.h"
#include "../../Scene/GameObject.h"
#include "../../Scene/Transform.h"
#include "../Camera.h"
#include "../Lighting/LightingProjectSettings.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace RTBEngine {
    namespace Rendering {

        namespace {
            FogFrameState s_currentFrameState{};

            struct VolumeBlendEntry {
                const Scene::VolumeComponent* volume = nullptr;
                float influence = 0.0f;
            };

            float ComputeBoxInfluence(const Scene::VolumeComponent& volume,
                                      const Math::Vector3& worldCameraPos)
            {
                if (volume.isGlobal) {
                    return 1.0f;
                }

                const Scene::GameObject* owner = volume.GetOwner();
                if (!owner) {
                    return 0.0f;
                }

                const Math::Matrix4 invWorld = owner->GetWorldMatrix().Inverse();
                const Math::Vector4 local4 = invWorld * Math::Vector4(
                    worldCameraPos.x, worldCameraPos.y, worldCameraPos.z, 1.0f);
                const Math::Vector3 local(local4.x, local4.y, local4.z);

                const Math::Vector3 half = volume.size * 0.5f;
                const Math::Vector3 distOutside(
                    std::max(std::fabs(local.x) - half.x, 0.0f),
                    std::max(std::fabs(local.y) - half.y, 0.0f),
                    std::max(std::fabs(local.z) - half.z, 0.0f));

                const float outsideDist = std::sqrt(
                    distOutside.x * distOutside.x
                    + distOutside.y * distOutside.y
                    + distOutside.z * distOutside.z);

                const float blend = std::max(volume.blendDistance, 1e-3f);
                if (outsideDist >= blend) {
                    return 0.0f;
                }

                return 1.0f - (outsideDist / blend);
            }
        }

        FogFrameState VolumeStack::Evaluate(const Camera* camera, const Scene::Scene* scene)
        {
            s_currentFrameState = FogFrameState::FromProjectDefaults();

            if (!camera || !scene) {
                return s_currentFrameState;
            }

            std::vector<VolumeBlendEntry> entries;
            entries.reserve(8);

            const Math::Vector3 cameraPos = camera->GetPosition();
            for (Scene::Component* component : Scene::ComponentQuery::GetComponents<Scene::VolumeComponent>()) {
                auto* volume = dynamic_cast<Scene::VolumeComponent*>(component);
                if (!volume || !volume->IsEnabled()) {
                    continue;
                }

                const Scene::GameObject* owner = volume->GetOwner();
                if (!owner || !owner->IsActiveInHierarchy()) {
                    continue;
                }

                const float boxInfluence = ComputeBoxInfluence(*volume, cameraPos);
                const float influence = std::clamp(boxInfluence * volume->weight, 0.0f, 1.0f);
                if (influence <= 1e-5f) {
                    continue;
                }

                entries.push_back({ volume, influence });
            }

            std::sort(entries.begin(), entries.end(),
                [](const VolumeBlendEntry& a, const VolumeBlendEntry& b) {
                    if (!a.volume || !b.volume) {
                        return a.volume != nullptr;
                    }
                    if (a.volume->priority != b.volume->priority) {
                        return a.volume->priority < b.volume->priority;
                    }
                    return a.influence < b.influence;
                });

            for (const VolumeBlendEntry& entry : entries) {
                if (entry.volume) {
                    entry.volume->ToProfile().BlendInto(s_currentFrameState, entry.influence);
                }
            }

            return s_currentFrameState;
        }

        const FogFrameState& VolumeStack::GetCurrentFrameState()
        {
            return s_currentFrameState;
        }

    }
}
