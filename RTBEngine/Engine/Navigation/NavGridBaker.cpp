#include "NavGridBaker.h"

#include "../Scene/GameObject.h"
#include "../Scene/RigidBodyComponent.h"
#include "../Physics/PhysicsWorld.h"
#include "../Physics/RigidBody.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace RTBEngine {
    namespace Navigation {
        namespace {

            constexpr int kSampleCount = 5;
            constexpr int kHorizontalDirectionCount = 8;

            bool IsStaticObstacle(const Physics::PhysicsQueryHit& hit)
            {
                if (!hit.gameObject) {
                    return true;
                }

                if (auto* rigidBody = hit.gameObject->GetComponent<Scene::RigidBodyComponent>()) {
                    if (rigidBody->GetRigidBody()) {
                        return rigidBody->GetRigidBody()->GetType() == Physics::RigidBodyType::Static;
                    }
                }

                return true;
            }

            bool IsWalkableGroundHit(const Physics::PhysicsQueryHit& hit,
                                     float floorReferenceY,
                                     float maxStepHeight,
                                     float minGroundNormalY)
            {
                // Only static colliders count as ground or blocking geometry during bake.
                if (!IsStaticObstacle(hit)) {
                    return false;
                }

                if (hit.normal.y < minGroundNormalY) {
                    return false;
                }

                const float groundY = hit.point.y;
                if (groundY > floorReferenceY + maxStepHeight) {
                    return false;
                }

                if (groundY < floorReferenceY - maxStepHeight * 4.0f) {
                    return false;
                }

                return true;
            }

            Physics::PhysicsQueryOptions MakeQueryOptions(const Scene::GameObject* ignoredObject)
            {
                Physics::PhysicsQueryOptions options;
                options.ignoredObject = ignoredObject;
                options.ignoreIgnoredObjectHierarchy = true;
                options.ignoreTriggers = true;
                return options;
            }

            bool FindGroundAt(Physics::PhysicsWorld& physicsWorld,
                              float worldX,
                              float worldZ,
                              float probeTopY,
                              float probeBottomY,
                              float floorReferenceY,
                              const NavGridBakeSettings& settings,
                              Physics::PhysicsQueryHit& outGroundHit)
            {
                const Math::Vector3 rayStart(worldX, probeTopY, worldZ);
                const Math::Vector3 rayEnd(worldX, probeBottomY, worldZ);

                Physics::PhysicsQueryOptions options = MakeQueryOptions(nullptr);
                if (!physicsWorld.RaycastClosest(rayStart, rayEnd, outGroundHit, options)) {
                    return false;
                }

                return IsWalkableGroundHit(
                    outGroundHit,
                    floorReferenceY,
                    settings.maxStepHeight,
                    settings.minGroundNormalY);
            }

            bool HasOverheadObstruction(Physics::PhysicsWorld& physicsWorld,
                                        const Math::Vector3& feetPosition,
                                        float agentRadius,
                                        float clearanceHeight,
                                        const Scene::GameObject* ignoredGround)
            {
                // Vertical ray from feet to clearanceHeight — low ceilings mark the cell blocked.
                const Math::Vector3 start(
                    feetPosition.x,
                    feetPosition.y + agentRadius * 0.1f,
                    feetPosition.z);
                const Math::Vector3 end(
                    feetPosition.x,
                    feetPosition.y + clearanceHeight,
                    feetPosition.z);

                Physics::PhysicsQueryHit hit;
                if (!physicsWorld.RaycastClosest(start, end, hit, MakeQueryOptions(ignoredGround))) {
                    return false;
                }

                return IsStaticObstacle(hit);
            }

            bool HasHorizontalObstruction(Physics::PhysicsWorld& physicsWorld,
                                          const Math::Vector3& agentCenter,
                                          float agentRadius,
                                          const Scene::GameObject* ignoredGround)
            {
                // Eight horizontal rays at agent height, length = agentRadius.
                static const std::array<Math::Vector3, kHorizontalDirectionCount> directions = {{
                    Math::Vector3(1.0f, 0.0f, 0.0f),
                    Math::Vector3(-1.0f, 0.0f, 0.0f),
                    Math::Vector3(0.0f, 0.0f, 1.0f),
                    Math::Vector3(0.0f, 0.0f, -1.0f),
                    Math::Vector3(0.70710677f, 0.0f, 0.70710677f),
                    Math::Vector3(-0.70710677f, 0.0f, 0.70710677f),
                    Math::Vector3(0.70710677f, 0.0f, -0.70710677f),
                    Math::Vector3(-0.70710677f, 0.0f, -0.70710677f),
                }};

                const float sweepDistance = std::max(agentRadius, 0.05f);

                for (const Math::Vector3& direction : directions) {
                    const Math::Vector3 start = agentCenter;
                    const Math::Vector3 end(
                        agentCenter.x + direction.x * sweepDistance,
                        agentCenter.y,
                        agentCenter.z + direction.z * sweepDistance);

                    Physics::PhysicsQueryHit hit;
                    if (!physicsWorld.RaycastClosest(start, end, hit, MakeQueryOptions(ignoredGround))) {
                        continue;
                    }

                    if (IsStaticObstacle(hit)) {
                        return true;
                    }
                }

                return false;
            }

            bool IsSampleWalkable(Physics::PhysicsWorld& physicsWorld,
                                  float worldX,
                                  float worldZ,
                                  float probeTopY,
                                  float probeBottomY,
                                  float floorReferenceY,
                                  const NavGridBakeSettings& settings,
                                  float referenceGroundY,
                                  bool hasReferenceGroundY,
                                  float& outGroundY)
            {
                Physics::PhysicsQueryHit groundHit;
                if (!FindGroundAt(
                        physicsWorld,
                        worldX,
                        worldZ,
                        probeTopY,
                        probeBottomY,
                        floorReferenceY,
                        settings,
                        groundHit)) {
                    return false;
                }

                outGroundY = groundHit.point.y;

                if (hasReferenceGroundY &&
                    std::abs(outGroundY - referenceGroundY) > settings.maxStepHeight) {
                    return false;
                }

                const Math::Vector3 feetPosition(worldX, outGroundY, worldZ);
                const Math::Vector3 agentCenter(
                    worldX,
                    outGroundY + settings.agentRadius,
                    worldZ);

                if (HasOverheadObstruction(
                        physicsWorld,
                        feetPosition,
                        settings.agentRadius,
                        settings.clearanceHeight,
                        groundHit.gameObject)) {
                    return false;
                }

                if (HasHorizontalObstruction(
                        physicsWorld,
                        agentCenter,
                        settings.agentRadius,
                        groundHit.gameObject)) {
                    return false;
                }

                return true;
            }

            void BuildSampleOffsets(float cellSize,
                                    float agentRadius,
                                    std::array<Math::Vector3, kSampleCount>& outOffsets)
            {
                // Center + four inset corners; all five must pass for the cell to be walkable.
                const float halfCell = cellSize * 0.5f;
                const float inset = std::min(agentRadius, halfCell * 0.85f);

                outOffsets[0] = Math::Vector3(0.0f, 0.0f, 0.0f);
                outOffsets[1] = Math::Vector3(inset, 0.0f, inset);
                outOffsets[2] = Math::Vector3(inset, 0.0f, -inset);
                outOffsets[3] = Math::Vector3(-inset, 0.0f, inset);
                outOffsets[4] = Math::Vector3(-inset, 0.0f, -inset);
            }

        }

        bool NavGridBaker::Bake(NavGrid& grid,
                                Physics::PhysicsWorld& physicsWorld,
                                const NavGridBakeSettings& settings,
                                int* outWalkableCellCount) const
        {
            if (!grid.IsConfigured()) {
                if (outWalkableCellCount) {
                    *outWalkableCellCount = 0;
                }
                return false;
            }

            grid.ClearWalkability();

            const float cellSize = grid.GetCellSize();
            const float floorReferenceY = grid.GetOrigin().y;
            const float probeTopY = floorReferenceY + settings.groundProbeHeight;
            const float probeBottomY = floorReferenceY - settings.groundProbeDepth;

            std::array<Math::Vector3, kSampleCount> sampleOffsets;
            BuildSampleOffsets(cellSize, settings.agentRadius, sampleOffsets);

            int walkableCells = 0;

            // Per cell: ground ray down, ceiling ray up, eight horizontal clearance rays (x5 sample points).
            for (int z = 0; z < grid.GetHeight(); ++z) {
                for (int x = 0; x < grid.GetWidth(); ++x) {
                    Math::Vector3 cellCenter;
                    if (!grid.CellToWorld(x, z, cellCenter)) {
                        continue;
                    }

                    float referenceGroundY = 0.0f;
                    bool hasReferenceGroundY = false;
                    bool cellWalkable = true;

                    for (const Math::Vector3& offset : sampleOffsets) {
                        const float sampleX = cellCenter.x + offset.x;
                        const float sampleZ = cellCenter.z + offset.z;

                        float sampleGroundY = 0.0f;
                        if (!IsSampleWalkable(
                                physicsWorld,
                                sampleX,
                                sampleZ,
                                probeTopY,
                                probeBottomY,
                                floorReferenceY,
                                settings,
                                referenceGroundY,
                                hasReferenceGroundY,
                                sampleGroundY)) {
                            cellWalkable = false;
                            break;
                        }

                        if (!hasReferenceGroundY) {
                            referenceGroundY = sampleGroundY;
                            hasReferenceGroundY = true;
                        }
                    }

                    if (cellWalkable) {
                        grid.SetWalkable(x, z, true);
                        ++walkableCells;
                    }
                }
            }

            if (outWalkableCellCount) {
                *outWalkableCellCount = walkableCells;
            }

            return true;
        }

    }
}
