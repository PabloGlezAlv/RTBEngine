#include "RigidBodyComponent.h"
#include "../Reflection/PropertyMacros.h"
#include "GameObject.h"
#include "BoxColliderComponent.h"
#include "SphereColliderComponent.h"
#include "CapsuleColliderComponent.h"
#include "../Physics/BoxCollider.h"
#include "../Physics/SphereCollider.h"
#include "../Physics/CapsuleCollider.h"
#include "../Physics/PhysicsWorld.h"
#include "../Physics/PhysicsUtils.h"

namespace {
    template <typename ColliderComponent>
    void ClearDynamicColliderRef(ColliderComponent* collider, btRigidBody* bulletBody)
    {
        if (!collider || !bulletBody || collider->GetBulletCollisionObject() != bulletBody) {
            return;
        }

        collider->SetPhysicsWorld(nullptr);
        collider->SetBulletCollisionObject(nullptr, false);
    }

    void ClearDynamicColliderRefs(RTBEngine::ECS::GameObject* owner, btRigidBody* bulletBody)
    {
        if (!owner || !bulletBody) {
            return;
        }

        ClearDynamicColliderRef(owner->GetComponent<RTBEngine::ECS::BoxColliderComponent>(), bulletBody);
        ClearDynamicColliderRef(owner->GetComponent<RTBEngine::ECS::SphereColliderComponent>(), bulletBody);
        ClearDynamicColliderRef(owner->GetComponent<RTBEngine::ECS::CapsuleColliderComponent>(), bulletBody);
    }

    RTBEngine::Physics::PhysicsWorld* ResolvePhysicsWorld(
        RTBEngine::ECS::GameObject* owner,
        RTBEngine::Physics::RigidBody* rigidBody)
    {
        if (rigidBody && rigidBody->GetPhysicsWorld()) {
            return rigidBody->GetPhysicsWorld();
        }

        if (!owner) {
            return nullptr;
        }

        if (auto* boxCollider = owner->GetComponent<RTBEngine::ECS::BoxColliderComponent>()) {
            if (boxCollider->GetPhysicsWorld()) {
                return boxCollider->GetPhysicsWorld();
            }
        }

        if (auto* sphereCollider = owner->GetComponent<RTBEngine::ECS::SphereColliderComponent>()) {
            if (sphereCollider->GetPhysicsWorld()) {
                return sphereCollider->GetPhysicsWorld();
            }
        }

        if (auto* capsuleCollider = owner->GetComponent<RTBEngine::ECS::CapsuleColliderComponent>()) {
            if (capsuleCollider->GetPhysicsWorld()) {
                return capsuleCollider->GetPhysicsWorld();
            }
        }

        return nullptr;
    }

    btTransform BuildColliderTransform(RTBEngine::ECS::GameObject* owner, const RTBEngine::Math::Vector3& centerOffset)
    {
        btTransform btTrans;
        btTrans.setIdentity();

        if (!owner) {
            return btTrans;
        }

        RTBEngine::ECS::Transform& transform = owner->GetTransform();
        const RTBEngine::Math::Quaternion rotation = transform.GetRotation();
        const RTBEngine::Math::Vector3 worldCenter = transform.GetPosition() + (rotation * centerOffset);

        btTrans.setOrigin(RTBEngine::Physics::PhysicsUtils::ToBullet(worldCenter));
        btTrans.setRotation(RTBEngine::Physics::PhysicsUtils::ToBullet(rotation));
        return btTrans;
    }

    template <typename ColliderComponent, typename ColliderType>
    bool RegisterStaticCollider(
        RTBEngine::ECS::GameObject* owner,
        ColliderComponent* component,
        ColliderType* collider,
        RTBEngine::Physics::PhysicsWorld* physicsWorld)
    {
        if (!owner || !component || !collider || !physicsWorld) {
            return false;
        }

        if (component->GetBulletCollisionObject() || component->GetPhysicsWorld()) {
            return false;
        }

        btCollisionShape* shape = collider->GetCollisionShape();
        if (!shape) {
            return false;
        }

        btCollisionObject* collisionObj = new btCollisionObject();
        collisionObj->setCollisionShape(shape);
        collisionObj->setWorldTransform(BuildColliderTransform(owner, collider->GetCenter()));
        collisionObj->setUserPointer(owner);
        collisionObj->setCollisionFlags(collisionObj->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);

        if (component->IsTrigger()) {
            collisionObj->setCollisionFlags(collisionObj->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
        }

        physicsWorld->AddCollisionObject(collisionObj);
        component->SetBulletCollisionObject(collisionObj, true);
        component->SetPhysicsWorld(physicsWorld);
        return true;
    }

    bool RegisterRemainingColliderAsStatic(
        RTBEngine::ECS::GameObject* owner,
        RTBEngine::Physics::PhysicsWorld* physicsWorld)
    {
        if (!owner || !physicsWorld) {
            return false;
        }

        if (auto* boxCollider = owner->GetComponent<RTBEngine::ECS::BoxColliderComponent>()) {
            return RegisterStaticCollider(owner, boxCollider, boxCollider->GetBoxCollider(), physicsWorld);
        }

        if (auto* sphereCollider = owner->GetComponent<RTBEngine::ECS::SphereColliderComponent>()) {
            return RegisterStaticCollider(owner, sphereCollider, sphereCollider->GetSphereCollider(), physicsWorld);
        }

        if (auto* capsuleCollider = owner->GetComponent<RTBEngine::ECS::CapsuleColliderComponent>()) {
            return RegisterStaticCollider(owner, capsuleCollider, capsuleCollider->GetCapsuleCollider(), physicsWorld);
        }

        return false;
    }
}

namespace RTBEngine {
    namespace ECS {

        using ThisClass = RigidBodyComponent;
        RTB_REGISTER_COMPONENT(RigidBodyComponent)
            RTB_PROPERTY(mass)
            RTB_PROPERTY(friction)
            RTB_PROPERTY(restitution)
            RTB_PROPERTY_ENUM(bodyType, "Static", "Dynamic", "Kinematic")
        RTB_END_REGISTER(RigidBodyComponent)

        RigidBodyComponent::RigidBodyComponent()
            : Component(), rigidBody(nullptr)
        {
        }

        RigidBodyComponent::~RigidBodyComponent()
        {
        }

        void RigidBodyComponent::OnAwake()
        {
            // Create the RigidBody on first wake if it wasn't set by SceneLoader/ConfigureRigidBody.
            // This happens when a component is instantiated via Prefab::Instantiate (copy-paste / prefab drop).
            if (!rigidBody) {
                auto rb = std::make_unique<Physics::RigidBody>();
                rb->SetMass(mass);
                rb->SetFriction(friction);
                rb->SetRestitution(restitution);
                rb->SetType(bodyType);
                rigidBody = std::move(rb);
            }
            SyncProperties();
        }

        void RigidBodyComponent::OnStart()
        {
            SyncProperties();
        }

        void RigidBodyComponent::OnUpdate(float /*deltaTime*/)
        {
        }

        void RigidBodyComponent::OnDestroy()
        {
            if (!rigidBody) {
                return;
            }

            GameObject* gameObject = owner;
            Physics::PhysicsWorld* physicsWorld = ResolvePhysicsWorld(gameObject, rigidBody.get());
            btRigidBody* bulletBody = rigidBody->GetBulletRigidBody();

            if (bulletBody) {
                ClearDynamicColliderRefs(gameObject, bulletBody);
                rigidBody->ClearBulletRigidBody();
            }

            rigidBody->SetPhysicsWorld(nullptr);

            if (gameObject && !gameObject->IsBeingDestroyed() && physicsWorld) {
                RegisterRemainingColliderAsStatic(gameObject, physicsWorld);
            }
        }

        void RigidBodyComponent::OnValidate()
        {
            SyncProperties();
        }

        void RigidBodyComponent::SetRigidBody(std::unique_ptr<Physics::RigidBody> rb)
        {
            rigidBody = std::move(rb);
            if (rigidBody) {
                // Read back initial values?
                // Or just enforce members? Enforcing members is safer for Inspector sync.
                SyncProperties();
            }
        }

        void RigidBodyComponent::SyncProperties() {
            if (rigidBody) {
                rigidBody->SetMass(mass);
                rigidBody->SetFriction(friction);
                rigidBody->SetRestitution(restitution);
                rigidBody->SetType(bodyType);
            }
        }

    }
}
