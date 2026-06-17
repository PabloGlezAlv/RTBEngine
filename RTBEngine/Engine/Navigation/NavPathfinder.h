#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector3.h"
#include "NavGrid.h"
#include <cstdint>
#include <vector>

namespace RTBEngine {
    namespace Navigation {

        // Grid-based A* pathfinder with string pulling. Owned by NavPathService (single instance).
        // Search buffers are reused across calls to avoid per-query allocations on a 64x64 grid.
        class RTB_API NavPathfinder {
        public:
            // Returns world-space waypoints. May return a partial path when the goal is unreachable.
            bool FindPath(const NavGrid& grid,
                          const Math::Vector3& startWorld,
                          const Math::Vector3& goalWorld,
                          std::vector<Math::Vector3>& outWaypoints) const;

            // Grid string pulling: drop intermediate waypoints when cell LOS exists.
            void SmoothPath(const NavGrid& grid,
                            std::vector<Math::Vector3>& inOutWaypoints) const;

        private:
            static float OctileHeuristic(int dx, int dz);

            void EnsureSearchBuffers(int cellCount) const;
            bool RebuildWaypointsFromCells(const NavGrid& grid,
                                           const std::vector<int>& cellIndices,
                                           const Math::Vector3& startWorld,
                                           const Math::Vector3& goalWorld,
                                           std::vector<Math::Vector3>& outWaypoints) const;

            // A* scratch space — mutable so FindPath stays const while reusing memory.
            mutable std::vector<float> gScoreBuffer;
            mutable std::vector<int> cameFromBuffer;
            mutable std::vector<uint8_t> closedBuffer;
        };

    }
}
