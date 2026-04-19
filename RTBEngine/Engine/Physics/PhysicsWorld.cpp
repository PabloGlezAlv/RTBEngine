#include "PhysicsWorld.h"
#include "PhysicsUtils.h"
#include "../ECS/GameObject.h"

namespace {
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
}

namespace RTBEngine {
    namespace Physics {

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
                return false;
            }

            outHit.gameObject = ResolveHitGameObject(callback.m_collisionObject);
            outHit.point = PhysicsUtils::FromBullet(callback.m_hitPointWorld);
            outHit.normal = PhysicsUtils::FromBullet(callback.m_hitNormalWorld);
            outHit.fraction = callback.m_closestHitFraction;
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
                return false;
            }

            outHit.gameObject = ResolveHitGameObject(callback.m_hitCollisionObject);
            outHit.point = PhysicsUtils::FromBullet(callback.m_hitPointWorld);
            outHit.normal = PhysicsUtils::FromBullet(callback.m_hitNormalWorld);
            outHit.fraction = callback.m_closestHitFraction;
            return outHit.gameObject != nullptr;
        }

    }
}
