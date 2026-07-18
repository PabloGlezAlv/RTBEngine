#pragma once

#include "Entity.h"
#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

namespace RTBEngine {
    namespace ECS {

        class SparseSet {
        public:
            bool Contains(Entity entity) const
            {
                if (entity.generation == 0 || entity.index >= sparse.size()) {
                    return false;
                }

                const std::uint32_t denseIndex = sparse[entity.index];
                return denseIndex < dense.size() && dense[denseIndex].index == entity.index &&
                    dense[denseIndex].generation == entity.generation;
            }

            void Clear()
            {
                sparse.clear();
                dense.clear();
            }

            void Reserve(std::size_t capacity)
            {
                sparse.reserve(capacity);
                dense.reserve(capacity);
            }

            void Remove(Entity entity)
            {
                if (!Contains(entity)) {
                    return;
                }

                const std::uint32_t denseIndex = sparse[entity.index];
                const Entity movedEntity = dense.back();
                dense[denseIndex] = movedEntity;
                sparse[movedEntity.index] = denseIndex;
                dense.pop_back();
                sparse[entity.index] = 0;
            }

            void Insert(Entity entity)
            {
                if (entity.index >= sparse.size()) {
                    sparse.resize(entity.index + 1, 0);
                }

                const std::uint32_t denseIndex = static_cast<std::uint32_t>(dense.size());
                sparse[entity.index] = denseIndex;
                dense.push_back(entity);
            }

            const std::vector<Entity>& Entities() const { return dense; }

            std::uint32_t DenseIndex(Entity entity) const
            {
                return sparse[entity.index];
            }

        private:
            std::vector<std::uint32_t> sparse;
            std::vector<Entity> dense;
        };

        template<typename T>
        class ComponentStorage {
        public:
            bool Has(Entity entity) const { return entities.Contains(entity); }

            T& Get(Entity entity)
            {
                assert(Has(entity));
                return data[IndexOf(entity)];
            }

            const T& Get(Entity entity) const
            {
                return const_cast<ComponentStorage*>(this)->Get(entity);
            }

            T& At(std::size_t denseIndex)
            {
                return data[denseIndex];
            }

            const T& At(std::size_t denseIndex) const
            {
                return data[denseIndex];
            }

            std::size_t Size() const { return data.size(); }

            template<typename... Args>
            T& Emplace(Entity entity, Args&&... args)
            {
                if (!Has(entity)) {
                    entities.Insert(entity);
                    data.emplace_back(std::forward<Args>(args)...);
                } else {
                    data[IndexOf(entity)] = T(std::forward<Args>(args)...);
                }
                return Get(entity);
            }

            void Remove(Entity entity)
            {
                if (!Has(entity)) {
                    return;
                }

                const std::size_t removeIndex = IndexOf(entity);
                const std::size_t lastIndex = data.size() - 1;
                if (removeIndex != lastIndex) {
                    data[removeIndex] = std::move(data[lastIndex]);
                }
                data.pop_back();
                entities.Remove(entity);
            }

            void Clear()
            {
                entities.Clear();
                data.clear();
            }

            const std::vector<Entity>& Entities() const { return entities.Entities(); }

            T* TryGet(Entity entity)
            {
                if (!Has(entity)) {
                    return nullptr;
                }
                return &data[IndexOf(entity)];
            }

            const T* TryGet(Entity entity) const
            {
                return const_cast<ComponentStorage*>(this)->TryGet(entity);
            }

        private:
            std::size_t IndexOf(Entity entity) const
            {
                // O(1) sparse lookup.
                return static_cast<std::size_t>(entities.DenseIndex(entity));
            }

            SparseSet entities;
            std::vector<T> data;
        };

    }
}
