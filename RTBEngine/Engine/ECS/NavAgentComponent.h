#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Math/Vectors/Vector3.h"
#include "../Reflection/PropertyMacros.h"
#include <vector>

namespace RTBEngine {
    namespace Navigation {
        class NavGrid;
        class NavPathfinder;
    }

    namespace ECS {

#pragma warning(push)
#pragma warning(disable: 4251)
        // Per-actor navigation driver. Gameplay sets destinations; this component owns waypoints
        // and asks NavPathService to run A* on the active scene grid.
        class RTB_API NavAgentComponent : public Component {
        public:
            NavAgentComponent();
            ~NavAgentComponent() override;

            void OnAwake() override;
            void OnFixedUpdate(float fixedDeltaTime) override;
            void OnDestroy() override;
            void OnValidate() override;

            float recalcInterval = 0.5f;
            float targetMoveThreshold = 0.75f;
            float waypointReachDistance = 0.35f;

            void SetDestination(const Math::Vector3& worldDestination);
            void ClearDestination();
            // Forces an immediate path solve when waypoints are missing (spawn / chase kickoff).
            void EnsurePathReady();

            bool HasActivePath() const { return hasActivePath; }
            bool HasDestination() const { return hasDestination; }
            const Math::Vector3& GetDestination() const { return destination; }
            bool HasMoveDirection() const;
            Math::Vector3 GetPlanarMoveDirection(const Math::Vector3& ownerWorldPosition) const;

            const std::vector<Math::Vector3>& GetWaypoints() const { return waypoints; }
            int GetCurrentWaypointIndex() const { return currentWaypointIndex; }

            void ProcessPathRequest(const Navigation::NavGrid& grid,
                                    Navigation::NavPathfinder& pathfinder);

            RTB_COMPONENT(NavAgentComponent)

        private:
            void ClampSettings();
            void AdvanceWaypoint(const Math::Vector3& ownerWorldPosition);
            void RequestPathIfNeeded(const Math::Vector3& ownerWorldPosition);
            float PlanarDistance(const Math::Vector3& a, const Math::Vector3& b) const;

            Math::Vector3 destination = Math::Vector3(0.0f, 0.0f, 0.0f);
            bool hasDestination = false;
            Math::Vector3 lastRequestedDestination = Math::Vector3(0.0f, 0.0f, 0.0f);
            float recalcTimer = 0.0f;

            std::vector<Math::Vector3> waypoints;
            int currentWaypointIndex = 0;
            bool hasActivePath = false;
            bool pathRequestQueued = false;
        };
#pragma warning(pop)

    }
}
