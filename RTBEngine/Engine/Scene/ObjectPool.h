#pragma once

#include "../Math/Quaternions/Quaternion.h"
#include "../Math/Vectors/Vector3.h"
#include "../RTBEngineAPI.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace RTBEngine {
    namespace Scene {
        class GameObject;

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API ObjectPool {
        public:
            static constexpr int kDefaultMaxPoolSize = 32;

            static ObjectPool& GetInstance();

            ObjectPool(const ObjectPool&) = delete;
            ObjectPool& operator=(const ObjectPool&) = delete;

            static std::string ResolvePoolKey(const std::string& prefabRefOrPath);

            GameObject* Acquire(const std::string& prefabPath,
                                const Math::Vector3& position,
                                const Math::Quaternion& rotation);
            void Release(GameObject* instance);

            void Prewarm(const std::string& prefabPath, int count);
            void ClearUnused();
            void Clear();

            void SetDefaultMaxPoolSize(int maxSize);
            void SetMaxPoolSize(const std::string& prefabPath, int maxSize);
            int GetMaxPoolSize(const std::string& prefabPath) const;

            bool OwnsInstance(GameObject* instance) const;

            /// Safe during CRT teardown when the Meyers singleton may already be destroyed.
            static bool IsAlive();
            static void ClearIfAlive();

        private:
            struct PoolInstanceRecord {
                std::string poolKey;
                bool inFreeList = false;
            };

            ObjectPool();
            ~ObjectPool();

            GameObject* CreateInstance(const std::string& prefabPath,
                                       const Math::Vector3& position,
                                       const Math::Quaternion& rotation);
            void PrepareInstanceForAcquire(GameObject* instance);
            void RestartInstanceLifecycle(GameObject* instance);
            void PrepareInstanceForRelease(GameObject* instance);
            void DestroyPooledInstance(GameObject* instance);
            int ResolveMaxPoolSize(const std::string& prefabPath) const;

            int defaultMaxPoolSize = kDefaultMaxPoolSize;
            std::unordered_map<std::string, std::vector<GameObject*>> freeLists;
            std::unordered_map<GameObject*, PoolInstanceRecord> instanceRecords;
            std::unordered_map<std::string, int> maxPoolSizes;
        };
#pragma warning(pop)

    } // namespace Scene
} // namespace RTBEngine
