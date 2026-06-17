#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector3.h"
#include "NavGrid.h"
#include <vector>

namespace RTBEngine {
    namespace Physics {
        class PhysicsWorld;
    }

    namespace Navigation {

        class RTB_API NavPathfinder {
        public:
            // Computes a path over the baked NavGrid. Returns waypoints in world space.
            bool FindPath(const NavGrid& grid,
                          const Math::Vector3& startWorld,
                          const Math::Vector3& goalWorld,
                          std::vector<Math::Vector3>& outWaypoints,
                          Physics::PhysicsWorld* physicsWorld = nullptr) const;

            // Reduces waypoint count while staying on walkable cells (string pulling).
            void SmoothPath(const NavGrid& grid,
                            Physics::PhysicsWorld* physicsWorld,
                            std::vector<Math::Vector3>& inOutWaypoints) const;

        private:
            // Octile heuristic for 8-directional A*.
            static float OctileHeuristic(int dx, int dz);
        };

    }
}
