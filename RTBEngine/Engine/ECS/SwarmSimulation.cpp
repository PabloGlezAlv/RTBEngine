#include "SwarmSimulation.h"

#include "../Scene/GameObject.h"
#include "World.h"
#include "Components/ProjectileComponents.h"

#include <chrono>
#include <cmath>

namespace RTBEngine {
    namespace ECS {

        namespace {
            constexpr float kTwoPi = 6.28318530718f;

            void TickSwarmMotionSystem(World& world, float deltaTime)
            {
                if (deltaTime <= 0.0f) {
                    return;
                }

                const auto start = std::chrono::steady_clock::now();
                std::uint32_t activeCount = 0;

                world.ForEach<LocalTransform, SwarmMotion>(
                    [&](Entity entity, LocalTransform& transform, SwarmMotion& motion) {
                        ++activeCount;
                        motion.phase += motion.angularSpeed * deltaTime;
                        if (motion.phase > kTwoPi) {
                            motion.phase -= kTwoPi;
                        }

                        const float cosP = std::cos(motion.phase);
                        const float sinP = std::sin(motion.phase);
                        transform.position.x = motion.center.x + cosP * motion.orbitRadius;
                        transform.position.z = motion.center.z + sinP * motion.orbitRadius;
                        transform.position.y = motion.height
                            + std::sin(motion.phase * motion.bobSpeed) * motion.bobAmplitude;
                        transform.fixedHeight = transform.position.y;

                        // Optional GameObject proxy sync (projectile-style hybrid).
                        if (ProjectileVisualLink* link = world.TryGet<ProjectileVisualLink>(entity)) {
                            if (link->visual) {
                                link->visual->GetTransform().SetPosition(transform.position);
                            }
                        }
                    });

                const auto end = std::chrono::steady_clock::now();
                SwarmSimulationStats& stats = world.GetSwarmStats();
                stats.activeSwarmCount = activeCount;
                ++stats.simulationTicks;
                stats.lastSimulationMilliseconds =
                    std::chrono::duration<double, std::milli>(end - start).count();
            }
        }

        void RegisterSwarmSystems(World& world)
        {
            world.GetScheduler().Register(
                SystemPhase::Simulation,
                [](World& activeWorld, float deltaTime) {
                    TickSwarmMotionSystem(activeWorld, deltaTime);
                });
        }

        Entity CreateSwarmEntity(World& world,
                                 Scene::GameObject* visual,
                                 const LocalTransform& transform,
                                 const SwarmMotion& motion,
                                 const SwarmColor& color)
        {
            Entity entity = world.Create();
            world.Add<LocalTransform>(entity, transform);
            world.Add<SwarmMotion>(entity, motion);
            world.Add<SwarmColor>(entity, color);
            if (visual) {
                world.Add<ProjectileVisualLink>(entity, ProjectileVisualLink{ visual });
            }
            return entity;
        }

        void DestroySwarmEntity(World& world, Entity entity)
        {
            if (entity.IsValid()) {
                world.Destroy(entity);
            }
        }

    }
}
