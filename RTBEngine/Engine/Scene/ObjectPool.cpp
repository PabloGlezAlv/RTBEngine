#include "ObjectPool.h"

#include "../Core/Logger.h"
#include "../Core/ResourceManager.h"
#include "GameObject.h"
#include "PrefabRegistry.h"
#include "Scene.h"
#include "SceneManager.h"

#include <algorithm>

namespace {
    RTBEngine::Scene::ObjectPool* g_objectPoolInstance = nullptr;

    void SetHierarchyActive(RTBEngine::Scene::GameObject* root, bool active)
    {
        if (!root) {
            return;
        }

        root->SetActive(active);
        for (RTBEngine::Scene::GameObject* child : root->GetChildren()) {
            SetHierarchyActive(child, active);
        }
    }

    const RTBEngine::Scene::Prefab* ResolvePrefab(const std::string& prefabPath)
    {
        if (prefabPath.empty()) {
            return nullptr;
        }

        const RTBEngine::Scene::Prefab* prefab =
            RTBEngine::Scene::PrefabRegistry::GetInstance().GetByPath(prefabPath);
        if (!prefab) {
            prefab = RTBEngine::Scene::PrefabRegistry::GetInstance().Get(prefabPath);
        }

        return prefab;
    }
}

namespace RTBEngine {
    namespace Scene {

        ObjectPool& ObjectPool::GetInstance()
        {
            static ObjectPool instance;
            return instance;
        }

        bool ObjectPool::IsAlive()
        {
            return g_objectPoolInstance != nullptr;
        }

        void ObjectPool::ClearIfAlive()
        {
            if (g_objectPoolInstance) {
                g_objectPoolInstance->Clear();
            }
        }

        ObjectPool::ObjectPool()
        {
            g_objectPoolInstance = this;
        }

        ObjectPool::~ObjectPool()
        {
            if (g_objectPoolInstance == this) {
                g_objectPoolInstance = nullptr;
            }
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

            if (SceneManager::GetInstance().IsSceneUnloading()) {
                return;
            }

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
        }

        void ObjectPool::PrepareInstanceForRelease(GameObject* instance)
        {
            if (!instance) {
                return;
            }

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

            // Scene teardown owns GameObject destruction. Re-entering RemoveGameObject
            // from component OnDestroy/Finish while the scene is dying corrupts containers.
            if (SceneManager::GetInstance().IsSceneUnloading()) {
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

    } // namespace Scene
} // namespace RTBEngine
