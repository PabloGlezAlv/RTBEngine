#include "World.h"

#include <chrono>

namespace RTBEngine {
    namespace ECS {

        namespace {
            World* g_activeWorld = nullptr;
        }

        World* World::GetActive()
        {
            return g_activeWorld;
        }

        void World::SetActive(World* world)
        {
            g_activeWorld = world;
        }

        World::World() = default;

        World::~World()
        {
            if (g_activeWorld == this) {
                g_activeWorld = nullptr;
            }
            Clear();
        }

        Entity World::Create()
        {
            Entity entity{};
            if (!freeEntityIndices.empty()) {
                entity.index = freeEntityIndices.back();
                freeEntityIndices.pop_back();
                // Generation was already bumped on Destroy; reuse that value.
                entity.generation = entityGenerations[entity.index].generation;
                if (entity.generation == 0) {
                    entity.generation = 1;
                }
            } else {
                entity.index = static_cast<std::uint32_t>(entityGenerations.size());
                entity.generation = 1;
                entityGenerations.emplace_back();
            }

            entityGenerations[entity.index] = entity;
            ++aliveEntityCount;
            simulationStats.aliveEntityCount = aliveEntityCount;
            return entity;
        }

        void World::Destroy(Entity entity)
        {
            if (!IsAlive(entity)) {
                return;
            }

            RemoveAllComponents(entity);
            freeEntityIndices.push_back(entity.index);
            // Bump generation so the destroyed handle becomes invalid immediately.
            entityGenerations[entity.index].generation = entity.generation + 1;
            entityGenerations[entity.index].index = entity.index;
            if (aliveEntityCount > 0) {
                --aliveEntityCount;
            }
            simulationStats.aliveEntityCount = aliveEntityCount;
        }

        bool World::IsAlive(Entity entity) const
        {
            if (!entity.IsValid() || entity.index >= entityGenerations.size()) {
                return false;
            }

            const Entity& slot = entityGenerations[entity.index];
            return slot.generation == entity.generation && slot.generation != 0;
        }

        void World::Clear()
        {
            for (auto& storage : storages) {
                storage.second->Clear();
            }
            storages.clear();
            entityGenerations.clear();
            freeEntityIndices.clear();
            aliveEntityCount = 0;
            simulationStats = {};
            projectileStats = {};
            swarmStats = {};
        }

        void World::Tick(SystemPhase phase, float deltaTime)
        {
            if (phase == SystemPhase::Simulation) {
                const auto start = std::chrono::steady_clock::now();
                scheduler.Tick(phase, *this, deltaTime);
                const auto end = std::chrono::steady_clock::now();
                simulationStats.lastSimulationMilliseconds =
                    std::chrono::duration<double, std::milli>(end - start).count();
                simulationStats.aliveEntityCount = aliveEntityCount;
                return;
            }

            scheduler.Tick(phase, *this, deltaTime);
        }

        void World::RemoveAllComponents(Entity entity)
        {
            for (auto& storage : storages) {
                storage.second->RemoveEntity(entity);
            }
        }

    }
}
