#include "OcclusionFadeSystem.h"

#include "Scene.h"
#include "Occludable.h"
#include "OcclusionTarget.h"
#include "MeshRenderer.h"
#include "GameObject.h"
#include "../Rendering/Camera.h"
#include "../Rendering/Frustum.h"
#include "../Math/Math.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kSegmentEpsilon = 0.0001f;
    constexpr float kAlphaOpaqueThreshold = 0.999f;

    bool SegmentIntersectsAabbBeforeDistance(
        const RTBEngine::Math::Vector3& segmentStart,
        const RTBEngine::Math::Vector3& segmentEnd,
        RTBEngine::Math::Vector3 boxMin,
        RTBEngine::Math::Vector3 boxMax,
        float maxDistance)
    {
        RTBEngine::Math::Vector3 direction = segmentEnd - segmentStart;
        const float segmentLength = direction.Length();
        if (segmentLength <= kSegmentEpsilon || maxDistance <= kSegmentEpsilon) {
            return false;
        }

        direction = direction * (1.0f / segmentLength);

        float tMin = 0.0f;
        float tMax = std::min(segmentLength, maxDistance);

        const float origins[3] = { segmentStart.x, segmentStart.y, segmentStart.z };
        const float directions[3] = { direction.x, direction.y, direction.z };
        const float mins[3] = { boxMin.x, boxMin.y, boxMin.z };
        const float maxs[3] = { boxMax.x, boxMax.y, boxMax.z };

        for (int axis = 0; axis < 3; ++axis) {
            if (std::fabs(directions[axis]) < kSegmentEpsilon) {
                if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) {
                    return false;
                }
                continue;
            }

            float t1 = (mins[axis] - origins[axis]) / directions[axis];
            float t2 = (maxs[axis] - origins[axis]) / directions[axis];
            if (t1 > t2) {
                std::swap(t1, t2);
            }

            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) {
                return false;
            }
        }

        return tMax >= 0.0f && tMin <= maxDistance;
    }

    bool IsOccluderBlockingView(
        RTBEngine::ECS::Occludable* occludable,
        const RTBEngine::Math::Vector3& cameraPosition,
        const RTBEngine::Math::Vector3& focusPosition,
        float defaultBoundsPadding)
    {
        if (!occludable || !occludable->IsEnabled() || !occludable->occluderEnabled) {
            return false;
        }

        RTBEngine::ECS::GameObject* owner = occludable->GetOwner();
        RTBEngine::ECS::MeshRenderer* renderer = occludable->GetMeshRenderer();
        if (!owner || !renderer || !owner->IsActiveInHierarchy()) {
            return false;
        }

        RTBEngine::Math::Vector3 localMin;
        RTBEngine::Math::Vector3 localMax;
        renderer->GetCombinedAABB(localMin, localMax);

        RTBEngine::Math::Vector3 worldMin;
        RTBEngine::Math::Vector3 worldMax;
        RTBEngine::Rendering::Frustum::TransformAABB(
            owner->GetWorldMatrix(),
            localMin,
            localMax,
            worldMin,
            worldMax);

        const float padding = occludable->boundsPadding > 0.0f
            ? occludable->boundsPadding
            : defaultBoundsPadding;
        if (padding > 0.0f) {
            const RTBEngine::Math::Vector3 paddingVector(padding, padding, padding);
            worldMin = worldMin - paddingVector;
            worldMax = worldMax + paddingVector;
        }

        const float focusDistance = (focusPosition - cameraPosition).Length();
        if (focusDistance <= kSegmentEpsilon) {
            return false;
        }

        return SegmentIntersectsAabbBeforeDistance(
            cameraPosition,
            focusPosition,
            worldMin,
            worldMax,
            focusDistance);
    }

    void ApplyFadeAlpha(RTBEngine::ECS::Occludable* occludable, float alpha)
    {
        if (!occludable) {
            return;
        }

        const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
        occludable->SetCurrentFadeAlpha(clampedAlpha);

        RTBEngine::ECS::MeshRenderer* renderer = occludable->GetMeshRenderer();
        if (renderer) {
            renderer->SetOcclusionFadeAlpha(clampedAlpha);
        }
    }
}

namespace RTBEngine {
    namespace ECS {

        void OcclusionFadeSystem::Update(Scene* scene, const OcclusionFadeSettings& settings, float deltaTime)
        {
            if (!scene) {
                return;
            }

            const float step = std::clamp(settings.fadeSpeed * std::max(0.0f, deltaTime), 0.0f, 1.0f);
            const float targetOpaqueAlpha = 1.0f;
            const float targetOccludedAlpha = std::clamp(settings.occludedAlpha, 0.05f, 1.0f);

            Rendering::Camera* camera = scene->GetActiveCamera();
            if (!settings.enabled || !camera) {
                Reset(scene);
                return;
            }

            const Math::Vector3 cameraPosition = camera->GetPosition();
            const std::vector<OcclusionTarget*>& targets = scene->GetCachedOcclusionTargets();
            const std::vector<Occludable*>& occluders = scene->GetCachedOccludables();

            if (targets.empty()) {
                for (Occludable* occludable : occluders) {
                    if (!occludable) {
                        continue;
                    }

                    const float nextAlpha = Math::Lerp(occludable->GetCurrentFadeAlpha(), targetOpaqueAlpha, step);
                    ApplyFadeAlpha(occludable, nextAlpha);
                }
                return;
            }

            for (Occludable* occludable : occluders) {
                if (!occludable || !occludable->IsEnabled() || !occludable->occluderEnabled) {
                    continue;
                }

                bool blocksAnyTarget = false;
                for (OcclusionTarget* target : targets) {
                    if (!target || !target->IsActiveTarget()) {
                        continue;
                    }

                    if (IsOccluderBlockingView(
                            occludable,
                            cameraPosition,
                            target->GetFocusPosition(),
                            settings.boundsPadding)) {
                        blocksAnyTarget = true;
                        break;
                    }
                }

                const float targetAlpha = blocksAnyTarget ? targetOccludedAlpha : targetOpaqueAlpha;
                const float nextAlpha = Math::Lerp(occludable->GetCurrentFadeAlpha(), targetAlpha, step);
                ApplyFadeAlpha(occludable, nextAlpha);
            }
        }

        void OcclusionFadeSystem::Reset(Scene* scene)
        {
            if (!scene) {
                return;
            }

            for (Occludable* occludable : scene->GetCachedOccludables()) {
                ApplyFadeAlpha(occludable, 1.0f);
            }
        }

    }
}
