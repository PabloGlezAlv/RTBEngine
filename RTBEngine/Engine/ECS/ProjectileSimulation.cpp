#include "ProjectileSimulation.h"
#include "SwarmSimulation.h"
#include "World.h"

#include "../Physics/PhysicsWorld.h"
#include "../Scene/GameObject.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace RTBEngine {
    namespace ECS {

        namespace {
            constexpr float kDirectionEpsilon = 0.0001f;
            constexpr float kDistanceEpsilon = 0.0001f;

            bool HasPlanarDirection(const Math::Vector3& value)
            {
                return std::abs(value.x) > kDirectionEpsilon || std::abs(value.z) > kDirectionEpsilon;
            }

            void TickProjectileFlightSystem(World& world, float deltaTime)
            {
                if (deltaTime <= 0.0f) {
                    return;
                }

                const auto start = std::chrono::steady_clock::now();

                std::uint32_t activeCount = 0;
                world.ForEach<LocalTransform, ProjectileFlight, ProjectilePhysicsContext>(
                    [&](Entity entity,
                        LocalTransform& transform,
                        ProjectileFlight& flight,
                        ProjectilePhysicsContext& physicsContext) {
                        if (flight.pendingDestroy) {
                            return;
                        }

                        ++activeCount;

                        const float remainingDistance = flight.maxDistance - flight.distanceTravelled;
                        if (remainingDistance <= kDistanceEpsilon) {
                            flight.pendingDestroy = true;
                            return;
                        }

                        const float stepDistance = std::min(flight.speed * deltaTime, remainingDistance);
                        if (stepDistance <= kDistanceEpsilon) {
                            return;
                        }

                        Math::Vector3 previousPosition = transform.position;
                        previousPosition.y = transform.fixedHeight;

                        Math::Vector3 nextPosition = previousPosition + flight.direction * stepDistance;
                        nextPosition.y = transform.fixedHeight;

                        ProjectilePendingHit* pendingHit = world.TryGet<ProjectilePendingHit>(entity);
                        if (!pendingHit) {
                            pendingHit = &world.Add<ProjectilePendingHit>(entity);
                        }
                        pendingHit->active = false;

                        bool shouldStop = false;
                        Math::Vector3 resolvedPosition = nextPosition;

                        if (physicsContext.physicsWorld) {
                            Physics::PhysicsQueryHit hit;
                            Physics::PhysicsQueryOptions options;
                            options.ignoredObject = physicsContext.instigator;
                            options.ignoreIgnoredObjectHierarchy = true;
                            options.ignoreTriggers = true;

                            if (physicsContext.physicsWorld->SphereCastClosest(
                                    previousPosition,
                                    nextPosition,
                                    flight.radius,
                                    hit,
                                    options)) {
                                const float hitFraction = std::clamp(hit.fraction, 0.0f, 1.0f);
                                resolvedPosition =
                                    previousPosition + (nextPosition - previousPosition) * hitFraction;
                                resolvedPosition.y = transform.fixedHeight;

                                pendingHit->active = true;
                                pendingHit->hitObject = hit.gameObject;
                                pendingHit->hitPoint = hit.point;
                                pendingHit->hitFraction = hitFraction;
                                shouldStop = true;
                            }
                        }

                        transform.position = shouldStop ? resolvedPosition : nextPosition;
                        transform.position.y = transform.fixedHeight;
                        flight.distanceTravelled =
                            std::min(flight.maxDistance, flight.distanceTravelled + stepDistance);
                        flight.shouldStop = shouldStop;

                        if (!shouldStop &&
                            flight.distanceTravelled + kDistanceEpsilon >= flight.maxDistance) {
                            flight.pendingDestroy = true;
                        }

                        (void)entity;
                    });

                const auto end = std::chrono::steady_clock::now();
                ProjectileSimulationStats& stats = world.GetProjectileStats();
                stats.activeProjectileCount = activeCount;
                ++stats.simulationTicks;
                stats.lastSimulationMilliseconds =
                    std::chrono::duration<double, std::milli>(end - start).count();
            }

            void TickProjectilePresentationSystem(World& world, float /*deltaTime*/)
            {
                // Only projectile visuals — swarm agents sync inside TickSwarmMotionSystem.
                world.ForEach<LocalTransform, ProjectileVisualLink, ProjectileFlight>(
                    [](Entity /*entity*/,
                       LocalTransform& transform,
                       ProjectileVisualLink& visualLink,
                       ProjectileFlight& /*flight*/) {
                        if (!visualLink.visual) {
                            return;
                        }

                        visualLink.visual->GetTransform().SetPosition(transform.position);
                    });
            }
        }

        void RegisterDefaultSystems(World& world)
        {
            world.GetScheduler().Register(
                SystemPhase::Simulation,
                [](World& activeWorld, float deltaTime) {
                    TickProjectileFlightSystem(activeWorld, deltaTime);
                });

            world.GetScheduler().Register(
                SystemPhase::Presentation,
                [](World& activeWorld, float deltaTime) {
                    TickProjectilePresentationSystem(activeWorld, deltaTime);
                });

            RegisterSwarmSystems(world);
        }

        Entity CreateProjectileEntity(World& world,
                                      Scene::GameObject* visual,
                                      const LocalTransform& transform,
                                      const ProjectileFlight& flight,
                                      const ProjectilePhysicsContext& physicsContext)
        {
            Entity entity = world.Create();
            world.Add<LocalTransform>(entity, transform);
            world.Add<ProjectileFlight>(entity, flight);
            world.Add<ProjectilePhysicsContext>(entity, physicsContext);
            world.Add<ProjectilePendingHit>(entity);
            world.Add<ProjectileVisualLink>(entity, ProjectileVisualLink{ visual });
            return entity;
        }

        void DestroyProjectileEntity(World& world, Entity entity)
        {
            if (!entity.IsValid()) {
                return;
            }

            world.Destroy(entity);
        }

    }
}
