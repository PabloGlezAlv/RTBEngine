#include "PhysicsSystem.h"
#include "PhysicsUtils.h"
#include "../ECS/Transform.h"
#include "../ECS/GameObject.h"
#include "../ECS/RigidBodyComponent.h"
#include "CollisionInfo.h"
#include <BulletCollision/NarrowPhaseCollision/btPersistentManifold.h>

namespace RTBEngine {
    namespace Physics {

        PhysicsSystem::PhysicsSystem(PhysicsWorld* physicsWorld)
            : physicsWorld(physicsWorld)
        {
        }

        PhysicsSystem::~PhysicsSystem()
        {
        }

        void PhysicsSystem::Update(ECS::Scene* scene, float deltaTime)
        {
            if (!scene || !physicsWorld)
                return;

            // Sync transforms from GameObjects to physics (for kinematic bodies)
            SyncTransformsToPhysics(scene);

            // Step the physics simulation
            physicsWorld->Step(deltaTime);
            //Update collision state
            ProcessCollisions();

            // Sync transforms from physics back to GameObjects (for dynamic bodies)
            SyncPhysicsToTransforms(scene);
        }

        void PhysicsSystem::InitializeRigidBody(ECS::GameObject* gameObject, ECS::RigidBodyComponent* component)
        {
            if (!gameObject || !component || !physicsWorld)
                return;

            Physics::RigidBody* rigidBody = component->GetRigidBody();
            Physics::Collider* collider = component->GetCollider();

            if (!rigidBody || !collider)
                return;

            // Get collision shape from collider
            btCollisionShape* shape = collider->GetCollisionShape();
            if (!shape)
                return;

            // Get initial transform from GameObject
            ECS::Transform& transform = gameObject->GetTransform();
            btVector3 position = PhysicsUtils::ToBullet(transform.GetPosition());
            btQuaternion rotation = PhysicsUtils::ToBullet(transform.GetRotation());

            btTransform btTrans;
            btTrans.setIdentity();
            btTrans.setOrigin(position);
            btTrans.setRotation(rotation);

            // Create motion state
            btDefaultMotionState* motionState = new btDefaultMotionState(btTrans);

            // Calculate inertia
            float mass = rigidBody->GetMass();
            btVector3 inertia(0, 0, 0);
            if (mass > 0.0f) {
                shape->calculateLocalInertia(mass, inertia);
            }

            // Create btRigidBody
            btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, inertia);
            rbInfo.m_friction = rigidBody->GetFriction();
            rbInfo.m_restitution = rigidBody->GetRestitution();

            std::unique_ptr<btRigidBody> btBody = std::make_unique<btRigidBody>(rbInfo);

            // Set collision flags based on type
            if (rigidBody->GetType() == RigidBodyType::Static) {
                btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
            }
            else if (rigidBody->GetType() == RigidBodyType::Kinematic) {
                btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
                btBody->setActivationState(DISABLE_DEACTIVATION);
            }

            // Configure trigger
            if (collider->IsTrigger()) {
                btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            }

            // Set owner and user pointer for collision callbacks
            rigidBody->SetOwner(gameObject);
            btBody->setUserPointer(gameObject);

            // Add to physics world
            physicsWorld->AddRigidBody(btBody.get());

            // Store btRigidBody in RigidBody wrapper
            rigidBody->SetBulletRigidBody(std::move(btBody));
        }

        void PhysicsSystem::DestroyRigidBody(ECS::RigidBodyComponent* component)
        {
            if (!component || !physicsWorld)
                return;

            Physics::RigidBody* rigidBody = component->GetRigidBody();
            if (!rigidBody)
                return;

            btRigidBody* btBody = rigidBody->GetBulletRigidBody();
            if (btBody)
            {
                physicsWorld->RemoveRigidBody(btBody);
            }
        }

        void PhysicsSystem::SyncTransformsToPhysics(ECS::Scene* scene)
        {
            const auto& gameObjects = scene->GetGameObjects();

            for (const auto& gameObject : gameObjects)
            {
                if (!gameObject->IsActive())
                    continue;

                ECS::RigidBodyComponent* rbComp = gameObject->GetComponent<ECS::RigidBodyComponent>();
                if (!rbComp || !rbComp->HasRigidBody())
                    continue;

                Physics::RigidBody* rigidBody = rbComp->GetRigidBody();
                btRigidBody* btBody = rigidBody->GetBulletRigidBody();
                if (!btBody)
                    continue;

                // Only sync for kinematic bodies (static and kinematic don't move by physics)
                if (rigidBody->GetType() == RigidBodyType::Kinematic)
                {
                    ECS::Transform& transform = gameObject->GetTransform();
                    btVector3 position = PhysicsUtils::ToBullet(transform.GetPosition());
                    btQuaternion rotation = PhysicsUtils::ToBullet(transform.GetRotation());

                    btTransform btTrans;
                    btTrans.setIdentity();
                    btTrans.setOrigin(position);
                    btTrans.setRotation(rotation);

                    btBody->setWorldTransform(btTrans);
                }
            }
        }

        void PhysicsSystem::SyncPhysicsToTransforms(ECS::Scene* scene)
        {
            const auto& gameObjects = scene->GetGameObjects();

            for (const auto& gameObject : gameObjects)
            {
                if (!gameObject->IsActive())
                    continue;

                ECS::RigidBodyComponent* rbComp = gameObject->GetComponent<ECS::RigidBodyComponent>();
                if (!rbComp || !rbComp->HasRigidBody())
                    continue;

                Physics::RigidBody* rigidBody = rbComp->GetRigidBody();
                btRigidBody* btBody = rigidBody->GetBulletRigidBody();
                if (!btBody)
                    continue;

                // Only sync for dynamic bodies (they are moved by physics)
                if (rigidBody->GetType() == RigidBodyType::Dynamic)
                {
                    const btTransform& btTrans = btBody->getWorldTransform();
                    const btVector3& btPos = btTrans.getOrigin();
                    const btQuaternion& btRot = btTrans.getRotation();

                    Math::Vector3 position = PhysicsUtils::FromBullet(btPos);
                    Math::Quaternion rotation = PhysicsUtils::FromBullet(btRot);

                    ECS::Transform& transform = gameObject->GetTransform();
                    transform.SetPosition(position);
                    transform.SetRotation(rotation);
                }
            }
        }

        void PhysicsSystem::ProcessCollisions()
        {
            currentCollisions.clear();

            btDispatcher* dispatcher = physicsWorld->GetDispatcher();
            int numManifolds = dispatcher->getNumManifolds();

            for (int i = 0; i < numManifolds; i++)
            {
                btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
                if (manifold->getNumContacts() <= 0)
                    continue;

                const btCollisionObject* objA = manifold->getBody0();
                const btCollisionObject* objB = manifold->getBody1();

                ECS::GameObject* goA = static_cast<ECS::GameObject*>(objA->getUserPointer());
                ECS::GameObject* goB = static_cast<ECS::GameObject*>(objB->getUserPointer());

                if (!goA || !goB)
                    continue;

                // Check if either collider is a trigger
                ECS::RigidBodyComponent* rbCompA = goA->GetComponent<ECS::RigidBodyComponent>();
                ECS::RigidBodyComponent* rbCompB = goB->GetComponent<ECS::RigidBodyComponent>();

                bool isTriggerA = rbCompA && rbCompA->GetCollider() && rbCompA->GetCollider()->IsTrigger();
                bool isTriggerB = rbCompB && rbCompB->GetCollider() && rbCompB->GetCollider()->IsTrigger();
                bool isTrigger = isTriggerA || isTriggerB;

                // Get contact info from first contact point
                btManifoldPoint& pt = manifold->getContactPoint(0);
                btVector3 contactPoint = pt.getPositionWorldOnB();
                btVector3 contactNormal = pt.m_normalWorldOnB;

                // Create collision pair (always store in consistent order)
                CollisionPair pair;
                if (goA < goB) {
                    pair = { goA, goB, isTrigger };
                }
                else {
                    pair = { goB, goA, isTrigger };
                }
                currentCollisions.insert(pair);

                // Create collision info for each object
                CollisionInfo infoForA;
                infoForA.otherObject = goB;
                infoForA.contactPoint = PhysicsUtils::FromBullet(contactPoint);
                infoForA.contactNormal = PhysicsUtils::FromBullet(contactNormal);
                infoForA.penetrationDepth = pt.getDistance();

                CollisionInfo infoForB;
                infoForB.otherObject = goA;
                infoForB.contactPoint = PhysicsUtils::FromBullet(contactPoint);
                infoForB.contactNormal = PhysicsUtils::FromBullet(contactNormal) * -1.0f;
                infoForB.penetrationDepth = pt.getDistance();

                // Determine collision state
                bool wasColliding = previousCollisions.find(pair) != previousCollisions.end();

                if (!wasColliding) {
                    // Enter
                    NotifyCallbacks(goA, infoForA, isTrigger, CollisionState::Enter);
                    NotifyCallbacks(goB, infoForB, isTrigger, CollisionState::Enter);
                }
                else {
                    // Stay
                    NotifyCallbacks(goA, infoForA, isTrigger, CollisionState::Stay);
                    NotifyCallbacks(goB, infoForB, isTrigger, CollisionState::Stay);
                }
            }

            // Check for Exit (was colliding but not anymore)
            for (const auto& pair : previousCollisions)
            {
                if (currentCollisions.find(pair) == currentCollisions.end())
                {
                    CollisionInfo infoForA;
                    infoForA.otherObject = pair.objectB;

                    CollisionInfo infoForB;
                    infoForB.otherObject = pair.objectA;

                    NotifyCallbacks(pair.objectA, infoForA, pair.isTrigger, CollisionState::Exit);
                    NotifyCallbacks(pair.objectB, infoForB, pair.isTrigger, CollisionState::Exit);
                }
            }

            previousCollisions = currentCollisions;
        }

        void PhysicsSystem::NotifyCallbacks(ECS::GameObject* object, const CollisionInfo& info, bool isTrigger, CollisionState state)
        {
            if (!object)
                return;

            const auto& components = object->GetComponents();
            for (const auto& component : components)
            {
                if (isTrigger)
                {
                    switch (state)
                    {
                    case CollisionState::Enter: component->OnTriggerEnter(info); break;
                    case CollisionState::Stay:  component->OnTriggerStay(info);  break;
                    case CollisionState::Exit:  component->OnTriggerExit(info);  break;
                    }
                }
                else
                {
                    switch (state)
                    {
                    case CollisionState::Enter: component->OnCollisionEnter(info); break;
                    case CollisionState::Stay:  component->OnCollisionStay(info);  break;
                    case CollisionState::Exit:  component->OnCollisionExit(info);  break;
                    }
                }
            }
        }

    }
}
