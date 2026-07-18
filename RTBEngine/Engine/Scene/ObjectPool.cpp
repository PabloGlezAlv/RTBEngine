#include "ObjectPool.h"

#include "../Core/Logger.h"
#include "../Core/ResourceManager.h"
#include "GameObject.h"
#include "IPoolable.h"
#include "PrefabRegistry.h"
#include "Scene.h"
#include "SceneManager.h"

#include <algorithm>

namespace {
    void SetHierarchyActive(RTBEngine::ECS::GameObject* root, bool active)
    {
        if (!root) {
            return;
        }

        root->SetActive(active);
        for (RTBEngine::ECS::GameObject* child : root->GetChildren()) {
            SetHierarchyActive(child, active);
        }
    }

    void InvokePoolableCallbacks(RTBEngine::ECS::GameObject* root, bool acquire)
    {
        if (!root) {
            return;
        }

        for (const auto& component : root->GetComponents()) {
            if (!component) {
                continue;
            }

            if (auto* poolable = dynamic_cast<RTBEngine::ECS::IPoolable*>(component.get())) {
                if (acquire) {
                    poolable->OnPoolAcquire();
                } else {
                    poolable->OnPoolRelease();
                }
            }
        }

        for (RTBEngine::ECS::GameObject* child : root->GetChildren()) {
            InvokePoolableCallbacks(child, acquire);
        }
    }

    const RTBEngine::ECS::Prefab* ResolvePrefab(const std::string& prefabPath)
    {
        if (prefabPath.empty()) {
            return nullptr;
        }

        const RTBEngine::ECS::Prefab* prefab =
            RTBEngine::ECS::PrefabRegistry::GetInstance().GetByPath(prefabPath);
        if (!prefab) {
            prefab = RTBEngine::ECS::PrefabRegistry::GetInstance().Get(prefabPath);
        }

        return prefab;
    }
}

namespace RTBEngine {
    namespace ECS {

        ObjectPool& ObjectPool::GetInstance()
        {
            static ObjectPool instance;
            return instance;
        }

        std::string ObjectPool::ResolvePoolKey(const std::string& prefabRefOrPath)
        {
            if (prefabRefOrPath.empty()) {
                return {};
            }

            const std::string resolvedPath =
                Core::ResourceManager::GetInstance().ResolvePathForRead(prefabRefOrPath);
            if (!resolvedPath.empty() &&
                PrefabRegistry::GetInstance().GetByPath(resolvedPath)) {
                return resolvedPath;
            }

            if (PrefabRegistry::GetInstance().GetByPath(prefabRefOrPath)) {
                return prefabRefOrPath;
            }

            if (PrefabRegistry::GetInstance().Get(prefabRefOrPath)) {
                return prefabRefOrPath;
            }

            return resolvedPath.empty() ? prefabRefOrPath : resolvedPath;
        }

        void ObjectPool::SetDefaultMaxPoolSize(int maxSize)
        {
            defaultMaxPoolSize = std::max(0, maxSize);
        }

        void ObjectPool::SetMaxPoolSize(const std::string& prefabPath, int maxSize)
        {
            const std::string poolKey = ResolvePoolKey(prefabPath);
            if (poolKey.empty()) {
                return;
            }

            maxPoolSizes[poolKey] = std::max(0, maxSize);
        }

        int ObjectPool::GetMaxPoolSize(const std::string& prefabPath) const
        {
            return ResolveMaxPoolSize(ResolvePoolKey(prefabPath));
        }

        int ObjectPool::ResolveMaxPoolSize(const std::string& prefabPath) const
        {
            const auto it = maxPoolSizes.find(prefabPath);
            if (it != maxPoolSizes.end()) {
                return it->second;
            }

            return defaultMaxPoolSize;
        }

        GameObject* ObjectPool::CreateInstance(const std::string& prefabPath,
                                             const Math::Vector3& position,
                                             const Math::Quaternion& rotation)
        {
            const Prefab* prefab = ResolvePrefab(prefabPath);
            if (!prefab) {
                RTB_WARN("[ObjectPool] Prefab not found for pool key '" + prefabPath + "'.");
                return nullptr;
            }

            GameObject* instance =
                SceneManager::GetInstance().Instantiate(*prefab, position, rotation);
            if (!instance) {
                return nullptr;
            }

            instance->SetTransient(true);
            instanceRecords[instance] = PoolInstanceRecord{ prefabPath, false };
            return instance;
        }

        void ObjectPool::DestroyPooledInstance(GameObject* instance)
        {
            if (!instance) {
                return;
            }

            instanceRecords.erase(instance);

            if (Scene* scene = SceneManager::GetInstance().GetActiveScene()) {
                scene->RemoveGameObject(instance);
            }
        }

        void ObjectPool::PrepareInstanceForAcquire(GameObject* instance)
        {
            if (!instance) {
                return;
            }

            SetHierarchyActive(instance, true);
            InvokePoolableCallbacks(instance, true);
        }

        void ObjectPool::PrepareInstanceForRelease(GameObject* instance)
        {
            if (!instance) {
                return;
            }

            InvokePoolableCallbacks(instance, false);
            SetHierarchyActive(instance, false);
        }

        GameObject* ObjectPool::Acquire(const std::string& prefabPath,
                                        const Math::Vector3& position,
                                        const Math::Quaternion& rotation)
        {
            const std::string poolKey = ResolvePoolKey(prefabPath);
            if (poolKey.empty()) {
                return nullptr;
            }

            Scene* scene = SceneManager::GetInstance().GetActiveScene();
            if (!scene) {
                return nullptr;
            }

            std::vector<GameObject*>& freeList = freeLists[poolKey];
            GameObject* instance = nullptr;

            while (!freeList.empty()) {
                instance = freeList.back();
                freeList.pop_back();

                auto recordIt = instanceRecords.find(instance);
                if (instance && recordIt != instanceRecords.end() && instance->GetOwningScene() == scene) {
                    recordIt->second.inFreeList = false;
                    instance->GetTransform().SetPosition(position);
                    instance->GetTransform().SetRotation(rotation);
                    PrepareInstanceForAcquire(instance);
                    return instance;
                }

                instanceRecords.erase(instance);
                instance = nullptr;
            }

            instance = CreateInstance(poolKey, position, rotation);
            if (!instance) {
                return nullptr;
            }

            PrepareInstanceForAcquire(instance);
            return instance;
        }

        void ObjectPool::Release(GameObject* instance)
        {
            if (!instance) {
                return;
            }

            const auto recordIt = instanceRecords.find(instance);
            if (recordIt == instanceRecords.end()) {
                if (Scene* scene = SceneManager::GetInstance().GetActiveScene()) {
                    scene->RemoveGameObject(instance);
                }
                return;
            }

            if (recordIt->second.inFreeList) {
                return;
            }

            const std::string& poolKey = recordIt->second.poolKey;
            std::vector<GameObject*>& freeList = freeLists[poolKey];
            const int maxPoolSize = ResolveMaxPoolSize(poolKey);
            if (maxPoolSize >= 0 &&
                static_cast<int>(freeList.size()) >= maxPoolSize) {
                DestroyPooledInstance(instance);
                return;
            }

            PrepareInstanceForRelease(instance);
            recordIt->second.inFreeList = true;
            freeList.push_back(instance);
        }

        void ObjectPool::Prewarm(const std::string& prefabPath, int count)
        {
            if (prefabPath.empty() || count <= 0) {
                return;
            }

            for (int i = 0; i < count; ++i) {
                GameObject* instance = Acquire(prefabPath, Math::Vector3::Zero(), Math::Quaternion::Identity());
                if (!instance) {
                    break;
                }

                Release(instance);
            }
        }

        void ObjectPool::ClearUnused()
        {
            Scene* scene = SceneManager::GetInstance().GetActiveScene();
            if (!scene) {
                freeLists.clear();
                instanceRecords.clear();
                return;
            }

            for (auto& entry : freeLists) {
                for (GameObject* instance : entry.second) {
                    instanceRecords.erase(instance);
                    scene->RemoveGameObject(instance);
                }
                entry.second.clear();
            }
        }

        void ObjectPool::Clear()
        {
            freeLists.clear();
            instanceRecords.clear();
            maxPoolSizes.clear();
            defaultMaxPoolSize = kDefaultMaxPoolSize;
        }

        bool ObjectPool::OwnsInstance(GameObject* instance) const
        {
            return instance && instanceRecords.find(instance) != instanceRecords.end();
        }

    } // namespace ECS
} // namespace RTBEngine
