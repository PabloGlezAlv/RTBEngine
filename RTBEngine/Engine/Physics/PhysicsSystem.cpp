#include "PhysicsSystem.h"
#include "PhysicsUtils.h"
#include "../ECS/Transform.h"

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

    }
}
