#include "NavAgentComponent.h"

#include "../Navigation/NavPathService.h"
#include "GameObject.h"
#include "../Math/Math.h"
#include "../Core/Logger.h"
#include "../Reflection/PropertyMacros.h"
#include <algorithm>

namespace RTBEngine {
    namespace ECS {

        using ThisClass = NavAgentComponent;
        RTB_REGISTER_COMPONENT(NavAgentComponent)
            RTB_PROPERTY_RANGE(recalcInterval, 0.05f, 5.0f)
            RTB_PROPERTY_RANGE(targetMoveThreshold, 0.1f, 5.0f)
            RTB_PROPERTY_RANGE(waypointReachDistance, 0.05f, 3.0f)
        RTB_END_REGISTER(NavAgentComponent)

        NavAgentComponent::NavAgentComponent() = default;

        NavAgentComponent::~NavAgentComponent()
        {
            Navigation::NavPathService::GetInstance().UnregisterAgent(this);
        }

        void NavAgentComponent::OnAwake()
        {
            ClampSettings();
            Navigation::NavPathService::GetInstance().RegisterAgent(this);
        }

        void NavAgentComponent::OnFixedUpdate(float fixedDeltaTime)
        {
            if (!owner || !IsEnabled() || !owner->IsActiveInHierarchy()) {
                return;
            }

            const Math::Vector3 ownerPosition = owner->GetWorldPosition();
            recalcTimer = std::max(0.0f, recalcTimer - fixedDeltaTime);

            if (hasDestination) {
                RequestPathIfNeeded(ownerPosition);
                AdvanceWaypoint(ownerPosition);
            }
        }

        void NavAgentComponent::OnDestroy()
        {
            Navigation::NavPathService::GetInstance().UnregisterAgent(this);
        }

        void NavAgentComponent::OnValidate()
        {
            ClampSettings();
        }

        void NavAgentComponent::SetDestination(const Math::Vector3& worldDestination)
        {
            const float targetMoved = PlanarDistance(worldDestination, lastRequestedDestination);
            destination = worldDestination;
            hasDestination = true;

            // Moving target invalidated the old polyline — schedule a fresh search.
            if (targetMoved >= targetMoveThreshold) {
                pathRequestQueued = false;
                hasActivePath = false;
                waypoints.clear();
                currentWaypointIndex = 0;
            }

            // First destination after spawn: solve synchronously so AI can move same frame.
            if (waypoints.empty() && Navigation::NavPathService::GetInstance().GetActiveGrid()) {
                pathRequestQueued = false;
                Navigation::NavPathService::GetInstance().ProcessAgentPathNow(this);
            }
        }

        void NavAgentComponent::ClearDestination()
        {
            hasDestination = false;
            hasActivePath = false;
            waypoints.clear();
            currentWaypointIndex = 0;
            pathRequestQueued = false;
        }

        void NavAgentComponent::EnsurePathReady()
        {
            if (!hasDestination || !owner) {
                return;
            }

            if (!Navigation::NavPathService::GetInstance().GetActiveGrid()) {
                return;
            }

            if (!waypoints.empty() && hasActivePath) {
                return;
            }

            pathRequestQueued = false;
            Navigation::NavPathService::GetInstance().ProcessAgentPathNow(this);
        }

        bool NavAgentComponent::HasMoveDirection() const
        {
            return hasActivePath && !waypoints.empty() &&
                   currentWaypointIndex < static_cast<int>(waypoints.size());
        }

        Math::Vector3 NavAgentComponent::GetPlanarMoveDirection(const Math::Vector3& ownerWorldPosition) const
        {
            if (!hasActivePath || waypoints.empty() ||
                currentWaypointIndex >= static_cast<int>(waypoints.size())) {
                return Math::Vector3::Zero();
            }

            const int targetIndex = std::clamp(
                currentWaypointIndex,
                0,
                static_cast<int>(waypoints.size()) - 1);

            Math::Vector3 toWaypoint =
                waypoints[static_cast<size_t>(targetIndex)] - ownerWorldPosition;
            toWaypoint.y = 0.0f;

            if (toWaypoint.LengthSquared() < 0.0001f) {
                return Math::Vector3::Zero();
            }

            return toWaypoint.Normalized();
        }

        void NavAgentComponent::ProcessPathRequest(const Navigation::NavGrid& grid,
                                                   Navigation::NavPathfinder& pathfinder)
        {
            pathRequestQueued = false;

            if (!owner || !hasDestination) {
                return;
            }

            std::vector<Math::Vector3> newWaypoints;
            const Math::Vector3 start = owner->GetWorldPosition();
            if (!pathfinder.FindPath(grid, start, destination, newWaypoints)) {
                hasActivePath = false;
                waypoints.clear();
                currentWaypointIndex = 0;
                return;
            }

            waypoints = std::move(newWaypoints);
            // Index 0 is the start cell; agents steer toward the next corner when possible.
            currentWaypointIndex = waypoints.size() > 1 ? 1 : 0;
            hasActivePath = !waypoints.empty();
            lastRequestedDestination = destination;
            recalcTimer = recalcInterval;

            Navigation::NavPathService::GetInstance().SetDebugAgent(this);
        }

        void NavAgentComponent::ClampSettings()
        {
            recalcInterval = std::max(recalcInterval, 0.05f);
            targetMoveThreshold = std::max(targetMoveThreshold, 0.1f);
            waypointReachDistance = std::max(waypointReachDistance, 0.05f);
        }

        void NavAgentComponent::AdvanceWaypoint(const Math::Vector3& ownerWorldPosition)
        {
            if (!hasActivePath || waypoints.empty()) {
                return;
            }

            while (currentWaypointIndex < static_cast<int>(waypoints.size())) {
                if (PlanarDistance(ownerWorldPosition, waypoints[static_cast<size_t>(currentWaypointIndex)]) >
                    waypointReachDistance) {
                    break;
                }

                ++currentWaypointIndex;
            }

            if (currentWaypointIndex >= static_cast<int>(waypoints.size())) {
                hasActivePath = false;
                return;
            }

            hasActivePath = !waypoints.empty();
        }

        void NavAgentComponent::RequestPathIfNeeded(const Math::Vector3& ownerWorldPosition)
        {
            if (!Navigation::NavPathService::GetInstance().GetActiveGrid()) {
                return;
            }

            const float targetMoved = PlanarDistance(destination, lastRequestedDestination);
            const bool targetMovedEnough = targetMoved >= targetMoveThreshold;
            const bool pathMissing = waypoints.empty();
            const bool pathExhausted =
                !waypoints.empty() &&
                currentWaypointIndex >= static_cast<int>(waypoints.size());
            const bool needsInitialPath = !hasActivePath && (pathMissing || pathExhausted);
            const bool timerElapsed = recalcTimer <= 0.0f;

            if (!needsInitialPath && !timerElapsed && !targetMovedEnough && !pathExhausted) {
                return;
            }

            (void)ownerWorldPosition;

            if (pathRequestQueued) {
                return;
            }

            pathRequestQueued = true;
            // Recurring target updates go through the per-frame budget in NavPathService.
            Navigation::NavPathService::GetInstance().QueuePathRequest(this);
        }

        float NavAgentComponent::PlanarDistance(const Math::Vector3& a, const Math::Vector3& b) const
        {
            const float dx = a.x - b.x;
            const float dz = a.z - b.z;
            return std::sqrt(dx * dx + dz * dz);
        }

    }
}
