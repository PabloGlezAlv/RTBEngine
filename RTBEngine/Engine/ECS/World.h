#pragma once

#include "../RTBEngineAPI.h"
#include "Components/ProjectileComponents.h"
#include "Components/SwarmComponents.h"
#include "EcsStats.h"
#include "Entity.h"
#include "SparseSet.h"
#include "SystemScheduler.h"
#include <cstdint>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace RTBEngine {
    namespace ECS {

        class RTB_API IComponentStorage {
        public:
            virtual ~IComponentStorage() = default;
            virtual void RemoveEntity(Entity entity) = 0;
            virtual void Clear() = 0;
        };

        template<typename T>
        class ComponentStorageAdapter : public IComponentStorage {
        public:
            ComponentStorage<T> storage;

            void RemoveEntity(Entity entity) override
            {
                storage.Remove(entity);
            }

            void Clear() override
            {
                storage.Clear();
            }
        };

        class RTB_API World {
        public:
            static World* GetActive();
            static void SetActive(World* world);

            World();
            ~World();

            World(const World&) = delete;
            World& operator=(const World&) = delete;

            Entity Create();
            void Destroy(Entity entity);
            bool IsAlive(Entity entity) const;
            void Clear();

            template<typename T>
            T& Add(Entity entity, T component = T{})
            {
                return GetStorage<T>().storage.Emplace(entity, std::move(component));
            }

            template<typename T>
            void Remove(Entity entity)
            {
                if (Has<T>(entity)) {
                    GetStorage<T>().storage.Remove(entity);
                }
            }

            template<typename T>
            bool Has(Entity entity) const
            {
                const auto it = storages.find(std::type_index(typeid(T)));
                if (it == storages.end()) {
                    return false;
                }

                const auto* adapter = static_cast<const ComponentStorageAdapter<T>*>(it->second.get());
                return adapter->storage.Has(entity);
            }

            template<typename T>
            T* TryGet(Entity entity)
            {
                auto it = storages.find(std::type_index(typeid(T)));
                if (it == storages.end()) {
                    return nullptr;
                }

                auto* adapter = static_cast<ComponentStorageAdapter<T>*>(it->second.get());
                return adapter->storage.TryGet(entity);
            }

            template<typename T>
            const T* TryGet(Entity entity) const
            {
                return const_cast<World*>(this)->TryGet<T>(entity);
            }

            template<typename T, typename Fn>
            void ForEach(Fn&& fn)
            {
                auto& storage = GetStorage<T>().storage;
                const auto& entities = storage.Entities();
                for (std::size_t i = 0; i < entities.size(); ++i) {
                    fn(entities[i], storage.At(i));
                }
            }

            template<typename T1, typename T2, typename Fn>
            void ForEach(Fn&& fn)
            {
                auto& primary = GetStorage<T1>().storage;
                auto& secondary = GetStorage<T2>().storage;
                const auto& entities = primary.Entities();
                for (std::size_t i = 0; i < entities.size(); ++i) {
                    Entity entity = entities[i];
                    if (T2* second = secondary.TryGet(entity)) {
                        fn(entity, primary.At(i), *second);
                    }
                }
            }

            template<typename T1, typename T2, typename T3, typename Fn>
            void ForEach(Fn&& fn)
            {
                auto& primary = GetStorage<T1>().storage;
                auto& secondary = GetStorage<T2>().storage;
                auto& tertiary = GetStorage<T3>().storage;
                const auto& entities = primary.Entities();
                for (std::size_t i = 0; i < entities.size(); ++i) {
                    Entity entity = entities[i];
                    T2* second = secondary.TryGet(entity);
                    T3* third = tertiary.TryGet(entity);
                    if (second && third) {
                        fn(entity, primary.At(i), *second, *third);
                    }
                }
            }

            SystemScheduler& GetScheduler() { return scheduler; }
            const SystemScheduler& GetScheduler() const { return scheduler; }

            void Tick(SystemPhase phase, float deltaTime);

            std::uint32_t GetAliveEntityCount() const { return aliveEntityCount; }

            EcsSimulationStats& GetSimulationStats() { return simulationStats; }
            const EcsSimulationStats& GetSimulationStats() const { return simulationStats; }

            ProjectileSimulationStats& GetProjectileStats() { return projectileStats; }
            const ProjectileSimulationStats& GetProjectileStats() const { return projectileStats; }

            SwarmSimulationStats& GetSwarmStats() { return swarmStats; }
            const SwarmSimulationStats& GetSwarmStats() const { return swarmStats; }

        private:
            template<typename T>
            ComponentStorageAdapter<T>& GetStorage()
            {
                const std::type_index typeKey(typeid(T));
                auto it = storages.find(typeKey);
                if (it == storages.end()) {
                    auto adapter = std::make_unique<ComponentStorageAdapter<T>>();
                    ComponentStorageAdapter<T>* raw = adapter.get();
                    storages.emplace(typeKey, std::move(adapter));
                    return *raw;
                }

                return *static_cast<ComponentStorageAdapter<T>*>(it->second.get());
            }

            template<typename T>
            const ComponentStorageAdapter<T>& GetStorage() const
            {
                return const_cast<World*>(this)->GetStorage<T>();
            }

            void RemoveAllComponents(Entity entity);

            std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> storages;
            std::vector<Entity> entityGenerations;
            std::vector<std::uint32_t> freeEntityIndices;
            SystemScheduler scheduler;
            std::uint32_t aliveEntityCount = 0;
            EcsSimulationStats simulationStats{};
            ProjectileSimulationStats projectileStats{};
            SwarmSimulationStats swarmStats{};
        };

    }
}
