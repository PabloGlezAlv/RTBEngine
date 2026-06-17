#include "NavPathfinder.h"

#include "../Physics/PhysicsWorld.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace RTBEngine {
    namespace Navigation {
        namespace {

            constexpr float kSqrt2 = 1.41421356f;

            struct AStarNode {
                int index = -1;
                float fScore = 0.0f;
            };

            struct AStarNodeCompare {
                bool operator()(const AStarNode& lhs, const AStarNode& rhs) const
                {
                    return lhs.fScore > rhs.fScore;
                }
            };

            struct NeighborOffset {
                int dx;
                int dz;
                float cost;
            };

            // 8-direction neighbors; diagonals cost sqrt(2).
            constexpr NeighborOffset kNeighbors[] = {
                { 1, 0, 1.0f },
                { -1, 0, 1.0f },
                { 0, 1, 1.0f },
                { 0, -1, 1.0f },
                { 1, 1, kSqrt2 },
                { 1, -1, kSqrt2 },
                { -1, 1, kSqrt2 },
                { -1, -1, kSqrt2 },
            };

            // Walks grid cells between (x0,z0) and (x1,z1) with Bresenham; all must be walkable.
            bool HasGridLineOfSight(const NavGrid& grid, int x0, int z0, int x1, int z1)
            {
                int currentX = x0;
                int currentZ = z0;

                const int deltaX = std::abs(x1 - x0);
                const int deltaZ = std::abs(z1 - z0);
                const int stepX = x0 < x1 ? 1 : -1;
                const int stepZ = z0 < z1 ? 1 : -1;
                int error = deltaX - deltaZ;

                while (true) {
                    if (!grid.IsWalkable(currentX, currentZ)) {
                        return false;
                    }

                    if (currentX == x1 && currentZ == z1) {
                        break;
                    }

                    const int errorTimesTwo = 2 * error;
                    if (errorTimesTwo > -deltaZ) {
                        error -= deltaZ;
                        currentX += stepX;
                    }

                    if (errorTimesTwo < deltaX) {
                        error += deltaX;
                        currentZ += stepZ;
                    }
                }

                return true;
            }

            bool HasGridLineOfSight(const NavGrid& grid,
                                    const Math::Vector3& fromWorld,
                                    const Math::Vector3& toWorld)
            {
                int fromX = 0;
                int fromZ = 0;
                int toX = 0;
                int toZ = 0;
                if (!grid.WorldToCell(fromWorld, fromX, fromZ) ||
                    !grid.WorldToCell(toWorld, toX, toZ)) {
                    return false;
                }

                return HasGridLineOfSight(grid, fromX, fromZ, toX, toZ);
            }

            float PlanarDistanceSquared(const Math::Vector3& a, const Math::Vector3& b)
            {
                const float dx = a.x - b.x;
                const float dz = a.z - b.z;
                return dx * dx + dz * dz;
            }

            bool IsWithinPlanarReach(const NavGrid& grid,
                                     const Math::Vector3& worldPosition,
                                     const Math::Vector3& reference,
                                     float maxCells)
            {
                const float maxDistance = grid.GetCellSize() * maxCells;
                return PlanarDistanceSquared(worldPosition, reference) <= maxDistance * maxDistance;
            }

            bool SnapToWalkableCell(const NavGrid& grid,
                                    int& inOutCellX,
                                    int& inOutCellZ,
                                    const Math::Vector3& referenceWorld,
                                    float maxSnapCells)
            {
                const int originalCellX = inOutCellX;
                const int originalCellZ = inOutCellZ;

                if (grid.IsWalkable(inOutCellX, inOutCellZ)) {
                    return true;
                }

                if (!grid.ClampToNearestWalkable(inOutCellX, inOutCellZ, 8)) {
                    return false;
                }

                Math::Vector3 snappedWorld;
                if (!grid.CellToWorld(inOutCellX, inOutCellZ, snappedWorld)) {
                    inOutCellX = originalCellX;
                    inOutCellZ = originalCellZ;
                    return false;
                }

                if (!IsWithinPlanarReach(grid, snappedWorld, referenceWorld, maxSnapCells)) {
                    inOutCellX = originalCellX;
                    inOutCellZ = originalCellZ;
                    return false;
                }

                return true;
            }

        }

        // Octile heuristic: admissible for 8-directional movement with costs 1 and sqrt(2).
        float NavPathfinder::OctileHeuristic(int dx, int dz)
        {
            const int absDx = std::abs(dx);
            const int absDz = std::abs(dz);
            const int minAxis = std::min(absDx, absDz);
            const int maxAxis = std::max(absDx, absDz);
            return static_cast<float>(minAxis) * kSqrt2 + static_cast<float>(maxAxis - minAxis);
        }

        bool NavPathfinder::FindPath(const NavGrid& grid,
                                     const Math::Vector3& startWorld,
                                     const Math::Vector3& goalWorld,
                                     std::vector<Math::Vector3>& outWaypoints,
                                     Physics::PhysicsWorld* /*physicsWorld*/) const
        {
            outWaypoints.clear();

            if (!grid.IsConfigured()) {
                return false;
            }

            int startX = 0;
            int startZ = 0;
            int goalX = 0;
            int goalZ = 0;
            if (!grid.WorldToCell(startWorld, startX, startZ) ||
                !grid.WorldToCell(goalWorld, goalX, goalZ)) {
                return false;
            }

            constexpr float kMaxSnapCells = 2.0f;
            if (!SnapToWalkableCell(grid, startX, startZ, startWorld, kMaxSnapCells) ||
                !SnapToWalkableCell(grid, goalX, goalZ, goalWorld, kMaxSnapCells)) {
                return false;
            }

            const int startIndex = grid.CellToIndex(startX, startZ);
            const int goalIndex = grid.CellToIndex(goalX, goalZ);

            if (startIndex == goalIndex) {
                Math::Vector3 waypoint;
                if (grid.CellToWorld(goalX, goalZ, waypoint)) {
                    waypoint.y = goalWorld.y;
                    outWaypoints.push_back(waypoint);
                }
                return true;
            }

            // A* over the baked grid.
            const int cellCount = grid.GetWidth() * grid.GetHeight();
            std::vector<float> gScore(static_cast<size_t>(cellCount), std::numeric_limits<float>::max());
            std::vector<int> cameFrom(static_cast<size_t>(cellCount), -1);
            std::vector<uint8_t> closed(static_cast<size_t>(cellCount), 0);

            std::priority_queue<AStarNode, std::vector<AStarNode>, AStarNodeCompare> openSet;
            gScore[static_cast<size_t>(startIndex)] = 0.0f;
            openSet.push({ startIndex, OctileHeuristic(goalX - startX, goalZ - startZ) });

            bool found = false;

            while (!openSet.empty()) {
                const AStarNode current = openSet.top();
                openSet.pop();

                if (closed[static_cast<size_t>(current.index)] != 0) {
                    continue;
                }

                if (current.index == goalIndex) {
                    found = true;
                    break;
                }

                closed[static_cast<size_t>(current.index)] = 1;

                int currentX = 0;
                int currentZ = 0;
                grid.IndexToCell(current.index, currentX, currentZ);

                for (const NeighborOffset& neighbor : kNeighbors) {
                    const int nextX = currentX + neighbor.dx;
                    const int nextZ = currentZ + neighbor.dz;
                    if (!grid.IsWalkable(nextX, nextZ)) {
                        continue;
                    }

                    // Prevent cutting corners on diagonal moves.
                    if (neighbor.dx != 0 && neighbor.dz != 0) {
                        if (!grid.IsWalkable(currentX + neighbor.dx, currentZ) ||
                            !grid.IsWalkable(currentX, currentZ + neighbor.dz)) {
                            continue;
                        }
                    }

                    const int nextIndex = grid.CellToIndex(nextX, nextZ);
                    if (closed[static_cast<size_t>(nextIndex)] != 0) {
                        continue;
                    }

                    const float tentativeG = gScore[static_cast<size_t>(current.index)] + neighbor.cost;
                    if (tentativeG >= gScore[static_cast<size_t>(nextIndex)]) {
                        continue;
                    }

                    cameFrom[static_cast<size_t>(nextIndex)] = current.index;
                    gScore[static_cast<size_t>(nextIndex)] = tentativeG;
                    const float fScore = tentativeG + OctileHeuristic(goalX - nextX, goalZ - nextZ);
                    openSet.push({ nextIndex, fScore });
                }
            }

            if (!found) {
                return false;
            }

            // Rebuild polyline from cell indices.
            std::vector<int> reversed;
            for (int current = goalIndex; current != -1; current = cameFrom[static_cast<size_t>(current)]) {
                reversed.push_back(current);
            }
            std::reverse(reversed.begin(), reversed.end());

            outWaypoints.reserve(reversed.size());
            for (int cellIndex : reversed) {
                int cellX = 0;
                int cellZ = 0;
                grid.IndexToCell(cellIndex, cellX, cellZ);

                Math::Vector3 waypoint;
                if (!grid.CellToWorld(cellX, cellZ, waypoint)) {
                    continue;
                }

                waypoint.y = startWorld.y;
                outWaypoints.push_back(waypoint);
            }

            if (!outWaypoints.empty()) {
                outWaypoints.back().y = goalWorld.y;
            }

            // Remove redundant waypoints while staying on walkable cells.
            SmoothPath(grid, nullptr, outWaypoints);

            if (outWaypoints.empty()) {
                return false;
            }

            constexpr float kMaxGoalReachCells = 2.5f;
            const Math::Vector3& pathEnd = outWaypoints.back();
            if (!IsWithinPlanarReach(grid, pathEnd, goalWorld, kMaxGoalReachCells)) {
                outWaypoints.clear();
                return false;
            }

            return true;
        }

        // Grid string pulling: skip intermediate waypoints when the cell line is walkable.
        void NavPathfinder::SmoothPath(const NavGrid& grid,
                                       Physics::PhysicsWorld* /*physicsWorld*/,
                                       std::vector<Math::Vector3>& inOutWaypoints) const
        {
            if (inOutWaypoints.size() < 3) {
                return;
            }

            std::vector<Math::Vector3> smoothed;
            smoothed.reserve(inOutWaypoints.size());
            smoothed.push_back(inOutWaypoints.front());

            size_t anchorIndex = 0;
            while (anchorIndex + 1 < inOutWaypoints.size()) {
                size_t farthestVisible = anchorIndex + 1;
                for (size_t candidate = anchorIndex + 2; candidate < inOutWaypoints.size(); ++candidate) {
                    if (HasGridLineOfSight(grid,
                                           inOutWaypoints[anchorIndex],
                                           inOutWaypoints[candidate])) {
                        farthestVisible = candidate;
                    }
                }

                smoothed.push_back(inOutWaypoints[farthestVisible]);
                anchorIndex = farthestVisible;
            }

            inOutWaypoints.swap(smoothed);
        }

    }
}
