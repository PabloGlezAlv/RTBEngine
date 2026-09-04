#include "PhysicsSystem.h"
#include "PhysicsLayerSettings.h"
#include "PhysicsUtils.h"
#include "BoxCollider.h"
#include "SphereCollider.h"
#include "CapsuleCollider.h"
#include "../Scene/SphereColliderComponent.h"
#include "../Scene/CapsuleColliderComponent.h"
#include "../Scene/Transform.h"
#include "../Scene/GameObject.h"
#include "../Scene/RigidBodyComponent.h"
#include "../Scene/BoxColliderComponent.h"
#include "../Math/Math.h"
#include "CollisionInfo.h"
#include <BulletCollision/NarrowPhaseCollision/btPersistentManifold.h>

namespace RTBEngine {
    namespace Physics {

        namespace {
            Math::Vector3 GetColliderCenterOffset(Scene::GameObject* gameObject)
            {
                if (!gameObject) {
                    return Math::Vector3(0.0f, 0.0f, 0.0f);
                }

                if (Scene::CapsuleColliderComponent* capsule = gameObject->GetComponent<Scene::CapsuleColliderComponent>()) {
                    return capsule->GetCenterOffset();
                }

                if (Scene::SphereColliderComponent* sphere = gameObject->GetComponent<Scene::SphereColliderComponent>()) {
                    return sphere->GetCenterOffset();
                }

                if (Scene::BoxColliderComponent* box = gameObject->GetComponent<Scene::BoxColliderComponent>()) {
                    return box->GetCenterOffset();
                }

                return Math::Vector3(0.0f, 0.0f, 0.0f);
            }

            btTransform BuildColliderTransform(Scene::Transform& transform, const Math::Vector3& centerOffset)
            {
                const Math::Quaternion rotation = transform.GetRotation();
                const Math::Vector3 worldCenter = transform.GetPosition() + (rotation * centerOffset);

                btTransform btTrans;
                btTrans.setIdentity();
                btTrans.setOrigin(PhysicsUtils::ToBullet(worldCenter));
                btTrans.setRotation(PhysicsUtils::ToBullet(rotation));
                return btTrans;
            }

            void GetCollisionFiltersForGameObject(
                const Scene::GameObject* gameObject,
                int& outGroup,
                int& outMask)
            {
                const PhysicsLayerSettings& settings = PhysicsLayerSettings::Get();
                const int layerIndex = gameObject ? gameObject->GetCollisionLayer() : 0;
                outGroup = settings.GetLayerGroup(layerIndex);
                outMask = settings.GetLayerMask(layerIndex);
            }

            void AddRigidBodyWithLayer(
                PhysicsWorld* world,
                Scene::GameObject* gameObject,
                btRigidBody* body)
            {
                int group = 0;
                int mask = 0;
                GetCollisionFiltersForGameObject(gameObject, group, mask);
                world->AddRigidBody(body, group, mask);
            }

            void AddCollisionObjectWithLayer(
                PhysicsWorld* world,
                Scene::GameObject* gameObject,
                btCollisionObject* object)
            {
                int group = 0;
                int mask = 0;
                GetCollisionFiltersForGameObject(gameObject, group, mask);
                world->AddCollisionObject(object, group, mask);
            }
        }

        PhysicsSystem::PhysicsSystem(PhysicsWorld* physicsWorld)
            : physicsWorld(physicsWorld)
        {
        }

        PhysicsSystem::~PhysicsSystem()
        {
        }

        void PhysicsSystem::Reset()
        {
            previousCollisions.clear();
            currentCollisions.clear();
        }

        void PhysicsSystem::Update(Scene::Scene* scene, float deltaTime)
        {
            if (!scene || !physicsWorld)
                return;

            SyncTransformsToPhysics(scene);
            physicsWorld->Step(deltaTime);
            ProcessCollisions();
            SyncPhysicsToTransforms(scene, 1.0f);
        }

        void PhysicsSystem::SyncRenderTransforms(Scene::Scene* scene, float interpolationAlpha)
        {
            if (!scene) {
                return;
            }

            SyncPhysicsToTransforms(scene, Math::Clamp01(interpolationAlpha));
        }

        void PhysicsSystem::InitializeCollider(Scene::GameObject* gameObject, Scene::BoxColliderComponent* boxCollider)
        {
            if (!gameObject || !boxCollider || !physicsWorld)
                return;

            Scene::RigidBodyComponent* rbComp = gameObject->GetComponent<Scene::RigidBodyComponent>();

            if (rbComp && rbComp->HasRigidBody()) {
                InitializeDynamicBody(gameObject, boxCollider, rbComp);
            }
            else {
                InitializeStaticCollider(gameObject, boxCollider);
            }
        }

        void PhysicsSystem::InitializeStaticCollider(Scene::GameObject* gameObject, Scene::BoxColliderComponent* boxCollider)
        {
            Physics::BoxCollider* collider = boxCollider->GetBoxCollider();
            if (!collider)
                return;

            btCollisionShape* shape = collider->GetCollisionShape();
            if (!shape)
                return;

            Scene::Transform& transform = gameObject->GetTransform();
            btTransform btTrans = BuildColliderTransform(transform, collider->GetCenter());

            btCollisionObject* collisionObj = new btCollisionObject();
            collisionObj->setCollisionShape(shape);
            collisionObj->setWorldTransform(btTrans);
            collisionObj->setUserPointer(gameObject);
            collisionObj->setCollisionFlags(collisionObj->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);

            if (boxCollider->IsTrigger()) {
                collisionObj->setCollisionFlags(collisionObj->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            }

            AddCollisionObjectWithLayer(physicsWorld, gameObject, collisionObj);
            boxCollider->SetBulletCollisionObject(collisionObj, true);
            boxCollider->SetPhysicsWorld(physicsWorld);
        }

        void PhysicsSystem::InitializeDynamicBody(Scene::GameObject* gameObject, Scene::BoxColliderComponent* boxCollider, Scene::RigidBodyComponent* rbComp)
        {
            Physics::BoxCollider* collider = boxCollider->GetBoxCollider();
            Physics::RigidBody* rigidBody = rbComp->GetRigidBody();

            if (!collider || !rigidBody)
                return;

            btCollisionShape* shape = collider->GetCollisionShape();
            if (!shape)
                return;

            Scene::Transform& transform = gameObject->GetTransform();
            btTransform btTrans = BuildColliderTransform(transform, collider->GetCenter());

            btDefaultMotionState* motionState = new btDefaultMotionState(btTrans);

            float mass = rigidBody->GetMass();
            btVector3 inertia(0, 0, 0);
            if (mass > 0.0f) {
                shape->calculateLocalInertia(mass, inertia);
            }

            btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, inertia);
            rbInfo.m_friction = rigidBody->GetFriction();
            rbInfo.m_restitution = rigidBody->GetRestitution();

            std::unique_ptr<btRigidBody> btBody = std::make_unique<btRigidBody>(rbInfo);

            if (rigidBody->GetType() == RigidBodyType::Static) {
                btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
            }
            else if (rigidBody->GetType() == RigidBodyType::Kinematic) {
                btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
                btBody->setActivationState(DISABLE_DEACTIVATION);
            }

            if (boxCollider->IsTrigger()) {
                btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            }

            rigidBody->SetOwner(gameObject);
            btBody->setUserPointer(gameObject);

            AddRigidBodyWithLayer(physicsWorld, gameObject, btBody.get());
            rigidBody->SetPhysicsWorld(physicsWorld);
            rigidBody->SetBulletRigidBody(std::move(btBody));
            boxCollider->SetBulletCollisionObject(rigidBody->GetBulletRigidBody());
            boxCollider->SetPhysicsWorld(physicsWorld);
        }

        void PhysicsSystem::SyncTransformsToPhysics(Scene::Scene* scene)
        {
            for (Scene::RigidBodyComponent* rbComp : scene->GetCachedRigidBodies())
            {
                Scene::GameObject* gameObject = rbComp ? rbComp->GetOwner() : nullptr;
                if (!gameObject || !gameObject->IsActiveInHierarchy())
                    continue;

                if (!rbComp->HasRigidBody())
                    continue;

                Physics::RigidBody* rigidBody = rbComp->GetRigidBody();
                btRigidBody* btBody = rigidBody->GetBulletRigidBody();
                if (!btBody)
                    continue;

                if (rigidBody->GetType() == RigidBodyType::Kinematic)
                {
                    Scene::Transform& transform = gameObject->GetTransform();
                    btTransform btTrans = BuildColliderTransform(transform, GetColliderCenterOffset(gameObject));

                    btBody->setWorldTransform(btTrans);
                }
            }
        }

        void PhysicsSystem::SyncPhysicsToTransforms(Scene::Scene* scene, float interpolationAlpha)
        {
            for (Scene::RigidBodyComponent* rbComp : scene->GetCachedRigidBodies())
            {
                Scene::GameObject* gameObject = rbComp ? rbComp->GetOwner() : nullptr;
                if (!gameObject || !gameObject->IsActiveInHierarchy())
                    continue;

                if (!rbComp->HasRigidBody())
                    continue;

                Physics::RigidBody* rigidBody = rbComp->GetRigidBody();
                btRigidBody* btBody = rigidBody->GetBulletRigidBody();
                if (!btBody)
                    continue;

                if (rigidBody->GetType() == RigidBodyType::Dynamic)
                {
                    btTransform sampledTransform = btBody->getWorldTransform();
                    if (interpolationAlpha < 1.0f) {
                        const btTransform& previousTransform = btBody->getInterpolationWorldTransform();
                        btQuaternion blendedRotation =
                            previousTransform.getRotation().slerp(sampledTransform.getRotation(), interpolationAlpha);
                        blendedRotation.normalize();

                        sampledTransform.setOrigin(
                            previousTransform.getOrigin().lerp(sampledTransform.getOrigin(), interpolationAlpha));
                        sampledTransform.setRotation(blendedRotation);
                    }

                    const btTransform& btTrans = sampledTransform;
                    const btVector3& btPos = btTrans.getOrigin();
                    const btQuaternion& btRot = btTrans.getRotation();

                    Math::Quaternion rotation = PhysicsUtils::FromBullet(btRot);
                    Math::Vector3 position = PhysicsUtils::FromBullet(btPos) -
                        (rotation * GetColliderCenterOffset(gameObject));

                    Scene::Transform& transform = gameObject->GetTransform();
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

                Scene::GameObject* goA = static_cast<Scene::GameObject*>(objA->getUserPointer());
                Scene::GameObject* goB = static_cast<Scene::GameObject*>(objB->getUserPointer());

                if (!goA || !goB)
                    continue;

                Scene::BoxColliderComponent* boxColliderA = goA->GetComponent<Scene::BoxColliderComponent>();
                Scene::BoxColliderComponent* boxColliderB = goB->GetComponent<Scene::BoxColliderComponent>();
                Scene::SphereColliderComponent* sphereColliderA = goA->GetComponent<Scene::SphereColliderComponent>();
                Scene::SphereColliderComponent* sphereColliderB = goB->GetComponent<Scene::SphereColliderComponent>();
                Scene::CapsuleColliderComponent* capsuleColliderA = goA->GetComponent<Scene::CapsuleColliderComponent>();
                Scene::CapsuleColliderComponent* capsuleColliderB = goB->GetComponent<Scene::CapsuleColliderComponent>();

                bool isTriggerA = (boxColliderA && boxColliderA->IsTrigger()) ||
                    (sphereColliderA && sphereColliderA->IsTrigger()) ||
                    (capsuleColliderA && capsuleColliderA->IsTrigger());
                bool isTriggerB = (boxColliderB && boxColliderB->IsTrigger()) ||
                    (sphereColliderB && sphereColliderB->IsTrigger()) ||
                    (capsuleColliderB && capsuleColliderB->IsTrigger());
                bool isTrigger = isTriggerA || isTriggerB;

                btManifoldPoint& pt = manifold->getContactPoint(0);
                btVector3 contactPoint = pt.getPositionWorldOnB();
                btVector3 contactNormal = pt.m_normalWorldOnB;

                CollisionPair pair;
                if (goA < goB) {
                    pair = { goA, goB, isTrigger };
                }
                else {
                    pair = { goB, goA, isTrigger };
                }
                currentCollisions.insert(pair);

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

                bool wasColliding = previousCollisions.find(pair) != previousCollisions.end();

                if (!wasColliding) {
                    NotifyCallbacks(goA, infoForA, isTrigger, CollisionState::Enter);
                    NotifyCallbacks(goB, infoForB, isTrigger, CollisionState::Enter);
                }
                else {
                    NotifyCallbacks(goA, infoForA, isTrigger, CollisionState::Stay);
                    NotifyCallbacks(goB, infoForB, isTrigger, CollisionState::Stay);
                }
            }

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

        void PhysicsSystem::NotifyCallbacks(Scene::GameObject* object, const CollisionInfo& info, bool isTrigger, CollisionState state)
        {
            if (!object)
                return;

            Scene::GameObject::ComponentIteration iteration(object);
            for (std::size_t i = 0; i < iteration.Count(); ++i)
            {
                Scene::Component* component = iteration.At(i);
                if (!component) {
                    continue;
                }

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

        void PhysicsSystem::InitializeCollider(Scene::GameObject* gameObject, Scene::SphereColliderComponent* sphereCollider)
        {
            if (!gameObject || !sphereCollider || !physicsWorld)
                return;

            Scene::RigidBodyComponent* rbComp = gameObject->GetComponent<Scene::RigidBodyComponent>();

            if (rbComp && rbComp->HasRigidBody()) {
                InitializeDynamicBody(gameObject, sphereCollider, rbComp);
            }
            else {
                InitializeStaticCollider(gameObject, sphereCollider);
            }
        }

        void PhysicsSystem::InitializeStaticCollider(Scene::GameObject* gameObject, Scene::SphereColliderComponent* sphereCollider)
        {
            Physics::SphereCollider* collider = sphereCollider->GetSphereCollider();
            if (!collider)
                return;

            btCollisionShape* shape = collider->GetCollisionShape();
            if (!shape)
                return;

            Scene::Transform& transform = gameObject->GetTransform();
            btTransform btTrans = BuildColliderTransform(transform, collider->GetCenter());

            btCollisionObject* collisionObj = new btCollisionObject();
            collisionObj->setCollisionShape(shape);
            collisionObj->setWorldTransform(btTrans);
            collisionObj->setUserPointer(gameObject);
            collisionObj->setCollisionFlags(collisionObj->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);

            if (sphereCollider->IsTrigger()) {
                collisionObj->setCollisionFlags(collisionObj->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            }

            AddCollisionObjectWithLayer(physicsWorld, gameObject, collisionObj);
            sphereCollider->SetBulletCollisionObject(collisionObj, true);
            sphereCollider->SetPhysicsWorld(physicsWorld);
        }

        void PhysicsSystem::InitializeDynamicBody(Scene::GameObject* gameObject, Scene::SphereColliderComponent* sphereCollider, Scene::RigidBodyComponent* rbComp)
        {
            Physics::SphereCollider* collider = sphereCollider->GetSphereCollider();
            Physics::RigidBody* rigidBody = rbComp->GetRigidBody();

            if (!collider || !rigidBody)
                return;

            btCollisionShape* shape = collider->GetCollisionShape();
            if (!shape)
                return;

            Scene::Transform& transform = gameObject->GetTransform();
            btTransform btTrans = BuildColliderTransform(transform, collider->GetCenter());

            btDefaultMotionState* motionState = new btDefaultMotionState(btTrans);

            float mass = rigidBody->GetMass();
            btVector3 inertia(0, 0, 0);
            if (mass > 0.0f) {
                shape->calculateLocalInertia(mass, inertia);
            }

            btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, inertia);
            rbInfo.m_friction = rigidBody->GetFriction();
            rbInfo.m_restitution = rigidBody->GetRestitution();

            std::unique_ptr<btRigidBody> btBody = std::make_unique<btRigidBody>(rbInfo);

            if (rigidBody->GetType() == RigidBodyType::Static) {
                btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
            }
            else if (rigidBody->GetType() == RigidBodyType::Kinematic) {
                btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
                btBody->setActivationState(DISABLE_DEACTIVATION);
            }

            if (sphereCollider->IsTrigger()) {
                btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            }

            rigidBody->SetOwner(gameObject);
            btBody->setUserPointer(gameObject);

            AddRigidBodyWithLayer(physicsWorld, gameObject, btBody.get());
            rigidBody->SetPhysicsWorld(physicsWorld);
            rigidBody->SetBulletRigidBody(std::move(btBody));
            sphereCollider->SetBulletCollisionObject(rigidBody->GetBulletRigidBody());
            sphereCollider->SetPhysicsWorld(physicsWorld);
        }

        void PhysicsSystem::InitializeCollider(Scene::GameObject* gameObject, Scene::CapsuleColliderComponent* capsuleCollider)
        {
            if (!gameObject || !capsuleCollider || !physicsWorld)
                return;

            Scene::RigidBodyComponent* rbComp = gameObject->GetComponent<Scene::RigidBodyComponent>();

            if (rbComp && rbComp->HasRigidBody()) {
                InitializeDynamicBody(gameObject, capsuleCollider, rbComp);
            }
            else {
                InitializeStaticCollider(gameObject, capsuleCollider);
            }
        }

        void PhysicsSystem::InitializeStaticCollider(Scene::GameObject* gameObject, Scene::CapsuleColliderComponent* capsuleCollider)
        {
            Physics::CapsuleCollider* collider = capsuleCollider->GetCapsuleCollider();
            if (!collider)
                return;

            btCollisionShape* shape = collider->GetCollisionShape();
            if (!shape)
                return;

            Scene::Transform& transform = gameObject->GetTransform();
            btTransform btTrans = BuildColliderTransform(transform, collider->GetCenter());

            btCollisionObject* collisionObj = new btCollisionObject();
            collisionObj->setCollisionShape(shape);
            collisionObj->setWorldTransform(btTrans);
            collisionObj->setUserPointer(gameObject);
            collisionObj->setCollisionFlags(collisionObj->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);

            if (capsuleCollider->IsTrigger()) {
                collisionObj->setCollisionFlags(collisionObj->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            }

            AddCollisionObjectWithLayer(physicsWorld, gameObject, collisionObj);
            capsuleCollider->SetBulletCollisionObject(collisionObj, true);
            capsuleCollider->SetPhysicsWorld(physicsWorld);
        }

        void PhysicsSystem::InitializeDynamicBody(Scene::GameObject* gameObject, Scene::CapsuleColliderComponent* capsuleCollider, Scene::RigidBodyComponent* rbComp)
        {
            Physics::CapsuleCollider* collider = capsuleCollider->GetCapsuleCollider();
            Physics::RigidBody* rigidBody = rbComp->GetRigidBody();

            if (!collider || !rigidBody)
                return;

            btCollisionShape* shape = collider->GetCollisionShape();
            if (!shape)
                return;

            Scene::Transform& transform = gameObject->GetTransform();
            btTransform btTrans = BuildColliderTransform(transform, collider->GetCenter());

            btDefaultMotionState* motionState = new btDefaultMotionState(btTrans);

            float mass = rigidBody->GetMass();
            btVector3 inertia(0, 0, 0);
            if (mass > 0.0f) {
                shape->calculateLocalInertia(mass, inertia);
            }

            btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, inertia);
            rbInfo.m_friction = rigidBody->GetFriction();
            rbInfo.m_restitution = rigidBody->GetRestitution();

            std::unique_ptr<btRigidBody> btBody = std::make_unique<btRigidBody>(rbInfo);

            if (rigidBody->GetType() == RigidBodyType::Static) {
                btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
            }
            else if (rigidBody->GetType() == RigidBodyType::Kinematic) {
                btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
                btBody->setActivationState(DISABLE_DEACTIVATION);
            }

            if (capsuleCollider->IsTrigger()) {
                btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            }

            rigidBody->SetOwner(gameObject);
            btBody->setUserPointer(gameObject);

            AddRigidBodyWithLayer(physicsWorld, gameObject, btBody.get());
            rigidBody->SetPhysicsWorld(physicsWorld);
            rigidBody->SetBulletRigidBody(std::move(btBody));
            capsuleCollider->SetBulletCollisionObject(rigidBody->GetBulletRigidBody());
            capsuleCollider->SetPhysicsWorld(physicsWorld);
        }

    }
}
