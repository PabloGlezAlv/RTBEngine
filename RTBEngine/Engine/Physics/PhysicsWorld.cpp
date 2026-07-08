#include "PhysicsWorld.h"
#include "PhysicsUtils.h"
#include "../Scene/GameObject.h"
#include <SDL_timer.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace {
    struct StoredPhysicsDebugQuery {
        RTBEngine::Physics::PhysicsDebugQueryType type = RTBEngine::Physics::PhysicsDebugQueryType::Raycast;
        RTBEngine::Math::Vector3 start = RTBEngine::Math::Vector3(0.0f, 0.0f, 0.0f);
        RTBEngine::Math::Vector3 end = RTBEngine::Math::Vector3(0.0f, 0.0f, 0.0f);
        float radius = 0.0f;
        bool hit = false;
        float hitFraction = 1.0f;
        Uint64 expiresAtMs = 0;
    };

    constexpr Uint64 kPhysicsDebugQueryLifetimeMs = 5000;
    constexpr std::size_t kMaxStoredPhysicsDebugQueries = 256;

    std::atomic_bool gPhysicsDebugQueriesEnabled{ false };
    std::mutex gPhysicsDebugQueriesMutex;
    std::vector<StoredPhysicsDebugQuery> gPhysicsDebugQueries;

    void PruneExpiredDebugQueriesLocked(Uint64 nowMs)
    {
        gPhysicsDebugQueries.erase(
            std::remove_if(
                gPhysicsDebugQueries.begin(),
                gPhysicsDebugQueries.end(),
                [nowMs](const StoredPhysicsDebugQuery& entry) {
                    return entry.expiresAtMs <= nowMs;
                }),
            gPhysicsDebugQueries.end());
    }

    void StorePhysicsDebugQuery(RTBEngine::Physics::PhysicsDebugQueryType type,
                                const RTBEngine::Math::Vector3& start,
                                const RTBEngine::Math::Vector3& end,
                                float radius,
                                bool hit,
                                float hitFraction)
    {
        if (!gPhysicsDebugQueriesEnabled.load(std::memory_order_relaxed)) {
            return;
        }

        const Uint64 nowMs = SDL_GetTicks64();

        std::lock_guard<std::mutex> lock(gPhysicsDebugQueriesMutex);
        PruneExpiredDebugQueriesLocked(nowMs);

        if (gPhysicsDebugQueries.size() >= kMaxStoredPhysicsDebugQueries) {
            const std::size_t overflow = gPhysicsDebugQueries.size() - kMaxStoredPhysicsDebugQueries + 1;
            gPhysicsDebugQueries.erase(gPhysicsDebugQueries.begin(),
                                       gPhysicsDebugQueries.begin() + static_cast<std::ptrdiff_t>(overflow));
        }

        StoredPhysicsDebugQuery entry;
        entry.type = type;
        entry.start = start;
        entry.end = end;
        entry.radius = radius;
        entry.hit = hit;
        entry.hitFraction = std::clamp(hitFraction, 0.0f, 1.0f);
        entry.expiresAtMs = nowMs + kPhysicsDebugQueryLifetimeMs;
        gPhysicsDebugQueries.push_back(entry);
    }

    RTBEngine::ECS::GameObject* ResolveHitGameObject(const btCollisionObject* collisionObject)
    {
        if (!collisionObject) {
            return nullptr;
        }

        return static_cast<RTBEngine::ECS::GameObject*>(collisionObject->getUserPointer());
    }

    bool IsSameOrDescendant(const RTBEngine::ECS::GameObject* candidate,
                            const RTBEngine::ECS::GameObject* root)
    {
        for (const RTBEngine::ECS::GameObject* current = candidate; current; current = current->GetParent()) {
            if (current == root) {
                return true;
            }
        }

        return false;
    }

    bool ShouldIgnoreCollisionObject(const btCollisionObject* collisionObject,
                                     const RTBEngine::Physics::PhysicsQueryOptions& options)
    {
        if (!collisionObject) {
            return false;
        }

        if (options.ignoreTriggers &&
            (collisionObject->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE) != 0) {
            return true;
        }

        if (!options.ignoredObject) {
            return false;
        }

        RTBEngine::ECS::GameObject* hitGameObject = ResolveHitGameObject(collisionObject);
        if (!hitGameObject) {
            return false;
        }

        if (options.ignoreIgnoredObjectHierarchy) {
            return IsSameOrDescendant(hitGameObject, options.ignoredObject);
        }

        return hitGameObject == options.ignoredObject;
    }

    class ClosestRayResultIgnoringObject final : public btCollisionWorld::ClosestRayResultCallback {
    public:
        ClosestRayResultIgnoringObject(const btVector3& rayFromWorld,
                                       const btVector3& rayToWorld,
                                       const RTBEngine::Physics::PhysicsQueryOptions& options)
            : btCollisionWorld::ClosestRayResultCallback(rayFromWorld, rayToWorld)
            , options(options)
        {
        }

        btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult,
                                 bool normalInWorldSpace) override
        {
            if (ShouldIgnoreCollisionObject(rayResult.m_collisionObject, options)) {
                return 1.0f;
            }

            return btCollisionWorld::ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
        }

    private:
        RTBEngine::Physics::PhysicsQueryOptions options;
    };

    class ClosestConvexResultIgnoringObject final : public btCollisionWorld::ClosestConvexResultCallback {
    public:
        ClosestConvexResultIgnoringObject(const btVector3& convexFromWorld,
                                          const btVector3& convexToWorld,
                                          const RTBEngine::Physics::PhysicsQueryOptions& options)
            : btCollisionWorld::ClosestConvexResultCallback(convexFromWorld, convexToWorld)
            , options(options)
        {
        }

        btScalar addSingleResult(btCollisionWorld::LocalConvexResult& convexResult,
                                 bool normalInWorldSpace) override
        {
            if (ShouldIgnoreCollisionObject(convexResult.m_hitCollisionObject, options)) {
                return 1.0f;
            }

            return btCollisionWorld::ClosestConvexResultCallback::addSingleResult(convexResult, normalInWorldSpace);
        }

    private:
        RTBEngine::Physics::PhysicsQueryOptions options;
    };

    class OverlapContactCallback final : public btCollisionWorld::ContactResultCallback {
    public:
        OverlapContactCallback(const btCollisionObject* queryObjectIn,
                               const RTBEngine::Math::Vector3& referencePointIn,
                               std::uint32_t layerMaskIn,
                               const RTBEngine::Physics::PhysicsQueryOptions& optionsIn)
            : queryObject(queryObjectIn)
            , referencePoint(referencePointIn)
            , options(optionsIn)
        {
            m_collisionFilterGroup = 1;
            m_collisionFilterMask = static_cast<int>(layerMaskIn);
        }

        void ReserveSeen(std::size_t count)
        {
            seenGameObjects.reserve(count);
        }

        void MarkSeen(RTBEngine::ECS::GameObject* gameObject)
        {
            if (gameObject) {
                seenGameObjects.insert(gameObject);
            }
        }

        btScalar addSingleResult(btManifoldPoint& contactPoint,
                                 const btCollisionObjectWrapper* wrapperA,
                                 int partIdA,
                                 int indexA,
                                 const btCollisionObjectWrapper* wrapperB,
                                 int partIdB,
                                 int indexB) override
        {
            (void)partIdA;
            (void)indexA;
            (void)partIdB;
            (void)indexB;

            const btCollisionObject* hitObject = wrapperA->getCollisionObject();
            if (hitObject == queryObject) {
                hitObject = wrapperB->getCollisionObject();
            }

            if (!hitObject || hitObject == queryObject) {
                return 0.0f;
            }

            if (ShouldIgnoreCollisionObject(hitObject, options)) {
                return 0.0f;
            }

            RTBEngine::ECS::GameObject* gameObject = ResolveHitGameObject(hitObject);
            if (!gameObject || seenGameObjects.find(gameObject) != seenGameObjects.end()) {
                return 0.0f;
            }

            seenGameObjects.insert(gameObject);

            RTBEngine::Physics::OverlapSphereHit hit;
            hit.gameObject = gameObject;
            hit.point = RTBEngine::Physics::PhysicsUtils::FromBullet(contactPoint.getPositionWorldOnB());
            hit.normal = RTBEngine::Physics::PhysicsUtils::FromBullet(contactPoint.m_normalWorldOnB);
            const RTBEngine::Math::Vector3 delta = hit.point - referencePoint;
            hit.distance = delta.Length();
            hits.push_back(hit);
            return 0.0f;
        }

        std::vector<RTBEngine::Physics::OverlapSphereHit> hits;

    private:
        const btCollisionObject* queryObject = nullptr;
        RTBEngine::Math::Vector3 referencePoint;
        RTBEngine::Physics::PhysicsQueryOptions options;
        std::unordered_set<RTBEngine::ECS::GameObject*> seenGameObjects;
    };

    class AllHitsConvexResultIgnoringObject final : public btCollisionWorld::ConvexResultCallback {
    public:
        AllHitsConvexResultIgnoringObject(const RTBEngine::Math::Vector3& referencePointIn,
                                          float segmentLengthIn,
                                          std::uint32_t layerMaskIn,
                                          const RTBEngine::Physics::PhysicsQueryOptions& optionsIn)
            : referencePoint(referencePointIn)
            , segmentLength(segmentLengthIn)
            , options(optionsIn)
        {
            m_collisionFilterGroup = 1;
            m_collisionFilterMask = static_cast<int>(layerMaskIn);
        }

        btScalar addSingleResult(btCollisionWorld::LocalConvexResult& convexResult,
                                 bool normalInWorldSpace) override
        {
            (void)normalInWorldSpace;

            const btCollisionObject* hitObject = convexResult.m_hitCollisionObject;
            if (!hitObject) {
                return 1.0f;
            }

            if (ShouldIgnoreCollisionObject(hitObject, options)) {
                return 1.0f;
            }

            RTBEngine::ECS::GameObject* gameObject = ResolveHitGameObject(hitObject);
            if (!gameObject || seenGameObjects.find(gameObject) != seenGameObjects.end()) {
                return 1.0f;
            }

            seenGameObjects.insert(gameObject);

            RTBEngine::Physics::OverlapSphereHit hit;
            hit.gameObject = gameObject;
            hit.point = RTBEngine::Physics::PhysicsUtils::FromBullet(convexResult.m_hitPointLocal);
            hit.normal = RTBEngine::Physics::PhysicsUtils::FromBullet(convexResult.m_hitNormalLocal);
            hit.distance = segmentLength > 0.0f
                ? convexResult.m_hitFraction * segmentLength
                : (hit.point - referencePoint).Length();
            hits.push_back(hit);
            return 1.0f;
        }

        std::vector<RTBEngine::Physics::OverlapSphereHit> hits;

    private:
        RTBEngine::Math::Vector3 referencePoint;
        float segmentLength = 0.0f;
        RTBEngine::Physics::PhysicsQueryOptions options;
        std::unordered_set<RTBEngine::ECS::GameObject*> seenGameObjects;
    };

    void ConfigureOverlapQueryObject(btCollisionObject& queryObject,
                                     btCollisionShape& shape,
                                     const btTransform& worldTransform)
    {
        queryObject.setCollisionShape(&shape);
        queryObject.setWorldTransform(worldTransform);
        queryObject.setCollisionFlags(
            queryObject.getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
        queryObject.setUserPointer(nullptr);
    }
}

namespace RTBEngine {
    namespace Physics {

        void SetPhysicsDebugQueriesEnabled(bool enabled)
        {
            gPhysicsDebugQueriesEnabled.store(enabled, std::memory_order_relaxed);

            if (!enabled) {
                ClearPhysicsDebugQueries();
            }
        }

        bool ArePhysicsDebugQueriesEnabled()
        {
            return gPhysicsDebugQueriesEnabled.load(std::memory_order_relaxed);
        }

        void ClearPhysicsDebugQueries()
        {
            std::lock_guard<std::mutex> lock(gPhysicsDebugQueriesMutex);
            gPhysicsDebugQueries.clear();
        }

        int GetPhysicsDebugQuerySnapshot(PhysicsDebugQueryEntry* outEntries, int maxEntries)
        {
            if (!outEntries || maxEntries <= 0) {
                return 0;
            }

            const Uint64 nowMs = SDL_GetTicks64();
            std::lock_guard<std::mutex> lock(gPhysicsDebugQueriesMutex);
            PruneExpiredDebugQueriesLocked(nowMs);

            const int availableCount = static_cast<int>(gPhysicsDebugQueries.size());
            const int resultCount = std::min(availableCount, maxEntries);
            const int startIndex = availableCount - resultCount;

            for (int i = 0; i < resultCount; ++i) {
                const StoredPhysicsDebugQuery& stored = gPhysicsDebugQueries[startIndex + i];
                PhysicsDebugQueryEntry entry;
                entry.type = stored.type;
                entry.start = stored.start;
                entry.end = stored.end;
                entry.radius = stored.radius;
                entry.hit = stored.hit;
                entry.hitFraction = stored.hitFraction;
                entry.remainingSeconds =
                    std::max(0.0f, static_cast<float>(stored.expiresAtMs - nowMs) / 1000.0f);
                outEntries[i] = entry;
            }

            return resultCount;
        }

        PhysicsWorld::PhysicsWorld()
            : collisionConfiguration(nullptr)
            , dispatcher(nullptr)
            , broadphase(nullptr)
            , solver(nullptr)
            , dynamicsWorld(nullptr)
        {
        }

        PhysicsWorld::~PhysicsWorld()
        {
            Cleanup();
        }

        void PhysicsWorld::Initialize()
        {
            // Create collision configuration
            collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();

            // Create collision dispatcher
            dispatcher = std::make_unique<btCollisionDispatcher>(collisionConfiguration.get());

            // Create broadphase interface
            broadphase = std::make_unique<btDbvtBroadphase>();

            // Create constraint solver
            solver = std::make_unique<btSequentialImpulseConstraintSolver>();

            // Create dynamics world
            dynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(
                dispatcher.get(),
                broadphase.get(),
                solver.get(),
                collisionConfiguration.get()
            );

            // Set default gravity (9.81 m/s^2 downward)
            dynamicsWorld->setGravity(btVector3(0.0f, -9.81f, 0.0f));
        }

        void PhysicsWorld::Step(float deltaTime)
        {
            if (dynamicsWorld)
            {
                // Step the simulation
                // maxSubSteps = 10, fixedTimeStep = 1/60
                dynamicsWorld->stepSimulation(deltaTime, 10, 1.0f / 60.0f);
            }
        }

        void PhysicsWorld::Cleanup()
        {
            // Remove all rigid bodies and collision objects from the world
            if (dynamicsWorld)
            {
                for (int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--)
                {
                    btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
                    btRigidBody* body = btRigidBody::upcast(obj);
                    if (body)
                        dynamicsWorld->removeRigidBody(body);
                    else
                        dynamicsWorld->removeCollisionObject(obj);
                }
            }

            // Clear all unique_ptr members (automatically deletes Bullet objects)
            dynamicsWorld.reset();
            solver.reset();
            broadphase.reset();
            dispatcher.reset();
            collisionConfiguration.reset();
        }

        void PhysicsWorld::ResetObjects()
        {
            if (!dynamicsWorld)
                return;

            // Only remove objects from the world — do NOT delete them.
            // Ownership of btRigidBody belongs to RigidBody (unique_ptr).
            // Ownership of static btCollisionObject belongs to BoxColliderComponent.
            for (int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--)
            {
                btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
                btRigidBody* body = btRigidBody::upcast(obj);
                if (body)
                    dynamicsWorld->removeRigidBody(body);
                else
                    dynamicsWorld->removeCollisionObject(obj);
            }
        }

        void PhysicsWorld::AddRigidBody(btRigidBody* body)
        {
            if (dynamicsWorld && body)
            {
                dynamicsWorld->addRigidBody(body);
            }
        }

        void PhysicsWorld::AddRigidBody(btRigidBody* body, int collisionFilterGroup, int collisionFilterMask)
        {
            if (dynamicsWorld && body)
            {
                dynamicsWorld->addRigidBody(body, collisionFilterGroup, collisionFilterMask);
            }
        }

        void PhysicsWorld::RemoveRigidBody(btRigidBody* body)
        {
            if (dynamicsWorld && body)
            {
                dynamicsWorld->removeRigidBody(body);
            }
        }

        void PhysicsWorld::AddCollisionObject(btCollisionObject* obj)
        {
            if (dynamicsWorld && obj)
            {
                dynamicsWorld->addCollisionObject(obj);
            }
        }

        void PhysicsWorld::AddCollisionObject(btCollisionObject* obj, int collisionFilterGroup, int collisionFilterMask)
        {
            if (dynamicsWorld && obj)
            {
                dynamicsWorld->addCollisionObject(obj, collisionFilterGroup, collisionFilterMask);
            }
        }

        void PhysicsWorld::RemoveCollisionObject(btCollisionObject* obj)
        {
            if (dynamicsWorld && obj)
            {
                dynamicsWorld->removeCollisionObject(obj);
            }
        }

        void PhysicsWorld::SetGravity(const Math::Vector3& gravity)
        {
            if (dynamicsWorld)
            {
                dynamicsWorld->setGravity(btVector3(gravity.x, gravity.y, gravity.z));
            }
        }

        Math::Vector3 PhysicsWorld::GetGravity() const
        {
            if (dynamicsWorld)
            {
                btVector3 gravity = dynamicsWorld->getGravity();
                return Math::Vector3(gravity.x(), gravity.y(), gravity.z());
            }
            return Math::Vector3(0.0f, 0.0f, 0.0f);
        }

        bool PhysicsWorld::RaycastClosest(const Math::Vector3& start,
                                          const Math::Vector3& end,
                                          PhysicsQueryHit& outHit,
                                          const PhysicsQueryOptions& options) const
        {
            outHit = {};

            if (!dynamicsWorld) {
                return false;
            }

            const btVector3 from = PhysicsUtils::ToBullet(start);
            const btVector3 to = PhysicsUtils::ToBullet(end);
            ClosestRayResultIgnoringObject callback(from, to, options);
            dynamicsWorld->rayTest(from, to, callback);

            if (!callback.hasHit()) {
                StorePhysicsDebugQuery(PhysicsDebugQueryType::Raycast, start, end, 0.0f, false, 1.0f);
                return false;
            }

            outHit.gameObject = ResolveHitGameObject(callback.m_collisionObject);
            outHit.point = PhysicsUtils::FromBullet(callback.m_hitPointWorld);
            outHit.normal = PhysicsUtils::FromBullet(callback.m_hitNormalWorld);
            outHit.fraction = callback.m_closestHitFraction;
            StorePhysicsDebugQuery(
                PhysicsDebugQueryType::Raycast,
                start,
                end,
                0.0f,
                true,
                outHit.fraction);
            return outHit.gameObject != nullptr;
        }

        bool PhysicsWorld::SphereCastClosest(const Math::Vector3& start,
                                             const Math::Vector3& end,
                                             float radius,
                                             PhysicsQueryHit& outHit,
                                             const PhysicsQueryOptions& options) const
        {
            outHit = {};

            if (!dynamicsWorld || radius <= 0.0f) {
                return false;
            }

            btSphereShape sphereShape(radius);
            btTransform fromTransform;
            fromTransform.setIdentity();
            fromTransform.setOrigin(PhysicsUtils::ToBullet(start));

            btTransform toTransform;
            toTransform.setIdentity();
            toTransform.setOrigin(PhysicsUtils::ToBullet(end));

            ClosestConvexResultIgnoringObject callback(
                fromTransform.getOrigin(),
                toTransform.getOrigin(),
                options);
            dynamicsWorld->convexSweepTest(&sphereShape, fromTransform, toTransform, callback);

            if (!callback.hasHit()) {
                StorePhysicsDebugQuery(PhysicsDebugQueryType::SphereCast, start, end, radius, false, 1.0f);
                return false;
            }

            outHit.gameObject = ResolveHitGameObject(callback.m_hitCollisionObject);
            outHit.point = PhysicsUtils::FromBullet(callback.m_hitPointWorld);
            outHit.normal = PhysicsUtils::FromBullet(callback.m_hitNormalWorld);
            outHit.fraction = callback.m_closestHitFraction;
            StorePhysicsDebugQuery(
                PhysicsDebugQueryType::SphereCast,
                start,
                end,
                radius,
                true,
                outHit.fraction);
            return outHit.gameObject != nullptr;
        }

        std::vector<OverlapSphereHit> PhysicsWorld::OverlapSphere(const Math::Vector3& center,
                                                                  float radius,
                                                                  std::uint32_t layerMask,
                                                                  const PhysicsQueryOptions& options) const
        {
            std::vector<OverlapSphereHit> results;

            if (!dynamicsWorld || radius <= 0.0f) {
                return results;
            }

            btSphereShape sphereShape(radius);
            btTransform worldTransform;
            worldTransform.setIdentity();
            worldTransform.setOrigin(PhysicsUtils::ToBullet(center));

            btCollisionObject queryObject;
            ConfigureOverlapQueryObject(queryObject, sphereShape, worldTransform);

            OverlapContactCallback callback(&queryObject, center, layerMask, options);
            dynamicsWorld->contactTest(&queryObject, callback);
            return callback.hits;
        }

        std::vector<OverlapSphereHit> PhysicsWorld::OverlapCapsuleSegment(const Math::Vector3& start,
                                                                          const Math::Vector3& end,
                                                                          float radius,
                                                                          std::uint32_t layerMask,
                                                                          const PhysicsQueryOptions& options) const
        {
            std::vector<OverlapSphereHit> results;

            if (!dynamicsWorld || radius <= 0.0f) {
                return results;
            }

            const Math::Vector3 delta = end - start;
            const float segmentLength = delta.Length();
            if (segmentLength <= 0.0f) {
                return OverlapSphere(start, radius, layerMask, options);
            }

            btSphereShape sphereShape(radius);
            btTransform fromTransform;
            fromTransform.setIdentity();
            fromTransform.setOrigin(PhysicsUtils::ToBullet(start));

            btTransform toTransform;
            toTransform.setIdentity();
            toTransform.setOrigin(PhysicsUtils::ToBullet(end));

            AllHitsConvexResultIgnoringObject callback(start, segmentLength, layerMask, options);
            dynamicsWorld->convexSweepTest(&sphereShape, fromTransform, toTransform, callback);

            results = std::move(callback.hits);

            btTransform endTransform;
            endTransform.setIdentity();
            endTransform.setOrigin(PhysicsUtils::ToBullet(end));
            btCollisionObject endQueryObject;
            ConfigureOverlapQueryObject(endQueryObject, sphereShape, endTransform);

            OverlapContactCallback endCapCallback(&endQueryObject, start, layerMask, options);
            endCapCallback.ReserveSeen(results.size());
            for (const OverlapSphereHit& existingHit : results) {
                endCapCallback.MarkSeen(existingHit.gameObject);
            }

            dynamicsWorld->contactTest(&endQueryObject, endCapCallback);
            results.insert(results.end(), endCapCallback.hits.begin(), endCapCallback.hits.end());
            return results;
        }

    }
}
