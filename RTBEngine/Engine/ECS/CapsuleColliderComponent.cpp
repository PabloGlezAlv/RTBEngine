#include "CapsuleColliderComponent.h"

#include "../Physics/CapsuleCollider.h"
#include "../Physics/PhysicsWorld.h"
#include "../Physics/PhysicsUtils.h"
#include "RigidBodyComponent.h"
#include "BoxColliderComponent.h"
#include "SphereColliderComponent.h"
#include "GameObject.h"
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <algorithm>

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

    void ClearSiblingDynamicColliderRefs(RTBEngine::ECS::GameObject* owner, btRigidBody* bulletBody)
    {
        if (!owner || !bulletBody) {
            return;
        }

        ClearDynamicColliderRef(owner->GetComponent<RTBEngine::ECS::BoxColliderComponent>(), bulletBody);
        ClearDynamicColliderRef(owner->GetComponent<RTBEngine::ECS::SphereColliderComponent>(), bulletBody);
        ClearDynamicColliderRef(owner->GetComponent<RTBEngine::ECS::CapsuleColliderComponent>(), bulletBody);
    }

    void SyncCapsuleShape(btCollisionObject* bulletObject,
                          RTBEngine::Physics::CapsuleCollider* collider,
                          RTBEngine::ECS::GameObject* owner)
    {
        if (!bulletObject || !collider) {
            return;
        }

        btCollisionShape* shape = collider->GetCollisionShape();
        bulletObject->setCollisionShape(shape);

        btRigidBody* body = btRigidBody::upcast(bulletObject);
        if (!body || !shape) {
            return;
        }

        float mass = 0.0f;
        if (owner) {
            auto* rbComp = owner->GetComponent<RTBEngine::ECS::RigidBodyComponent>();
            if (rbComp && rbComp->HasRigidBody() && rbComp->GetRigidBody()) {
                mass = rbComp->GetRigidBody()->GetMass();
            } else if (rbComp) {
                mass = rbComp->mass;
            }
        }

        btVector3 localInertia(0, 0, 0);
        if (mass > 0.0f) {
            shape->calculateLocalInertia(mass, localInertia);
        }

        body->setMassProps(mass, localInertia);
        body->updateInertiaTensor();
        body->activate(true);
    }

    void SyncCapsuleTransform(btCollisionObject* bulletObject,
                              RTBEngine::ECS::GameObject* owner,
                              const RTBEngine::Math::Vector3& centerOffset)
    {
        if (!bulletObject || !owner) {
            return;
        }

        RTBEngine::ECS::Transform& transform = owner->GetTransform();
        const RTBEngine::Math::Quaternion rotation = transform.GetRotation();
        const RTBEngine::Math::Vector3 centerPosition = transform.GetPosition() + (rotation * centerOffset);

        btTransform btTrans;
        btTrans.setIdentity();
        btTrans.setOrigin(RTBEngine::Physics::PhysicsUtils::ToBullet(centerPosition));
        btTrans.setRotation(RTBEngine::Physics::PhysicsUtils::ToBullet(rotation));

        bulletObject->setWorldTransform(btTrans);

        if (btRigidBody* body = btRigidBody::upcast(bulletObject)) {
            body->setCenterOfMassTransform(btTrans);
            body->setInterpolationWorldTransform(btTrans);
            if (btMotionState* motionState = body->getMotionState()) {
                motionState->setWorldTransform(btTrans);
            }
            body->activate(true);
        }
    }
}

namespace RTBEngine {
    namespace ECS {

        using ThisClass = CapsuleColliderComponent;
        RTB_REGISTER_COMPONENT(CapsuleColliderComponent)
            RTB_PROPERTY(radius)
            RTB_PROPERTY(height)
            RTB_PROPERTY(centerOffset)
            RTB_PROPERTY(isTrigger)
        RTB_END_REGISTER(CapsuleColliderComponent)

        CapsuleColliderComponent::CapsuleColliderComponent()
            : capsuleCollider(std::make_unique<Physics::CapsuleCollider>(radius, height))
        {
            capsuleCollider->SetCenter(centerOffset);
        }

        CapsuleColliderComponent::~CapsuleColliderComponent()
        {
            if (ownsBulletObject && bulletObject)
                delete bulletObject;
        }

        void CapsuleColliderComponent::OnDestroy()
        {
            if (owner) {
                RigidBodyComponent* rb = owner->GetComponent<RigidBodyComponent>();
                Physics::RigidBody* rigidBody = (rb && rb->HasRigidBody()) ? rb->GetRigidBody() : nullptr;
                btRigidBody* sharedBody = rigidBody ? rigidBody->GetBulletRigidBody() : nullptr;

                if (sharedBody && bulletObject == sharedBody) {
                    if (owner->IsBeingDestroyed()) {
                        SetPhysicsWorld(nullptr);
                        SetBulletCollisionObject(nullptr, false);
                        return;
                    }

                    ClearSiblingDynamicColliderRefs(owner, sharedBody);
                    rigidBody->ClearBulletRigidBody();
                    rigidBody->SetPhysicsWorld(nullptr);
                    return;
                }
            }

            if (bulletObject) {
                if (Physics::PhysicsWorld* world = GetPhysicsWorld()) {
                    world->RemoveCollisionObject(bulletObject);
                }

                SetPhysicsWorld(nullptr);
                SetBulletCollisionObject(nullptr, false);
            }
        }

        void CapsuleColliderComponent::OnValidate()
        {
            SetRadius(radius);
            SetHeight(height);
            SetCenterOffset(centerOffset);
            SetIsTrigger(isTrigger);
        }

        void CapsuleColliderComponent::SetBulletCollisionObject(btCollisionObject* obj, bool takeOwnership)
        {
            if (ownsBulletObject && bulletObject && bulletObject != obj) {
                delete bulletObject;
            }

            bulletObject = obj;
            ownsBulletObject = takeOwnership;
        }

        void CapsuleColliderComponent::SetRadius(float value)
        {
            radius = std::max(0.01f, value);
            height = std::max(height, radius * 2.0f);

            if (capsuleCollider) {
                capsuleCollider->SetRadius(radius);
                capsuleCollider->SetHeight(height);
            }

            SyncCapsuleShape(bulletObject, capsuleCollider.get(), owner);
            SyncCapsuleTransform(bulletObject, owner, centerOffset);
        }

        float CapsuleColliderComponent::GetRadius() const
        {
            if (capsuleCollider) {
                return capsuleCollider->GetRadius();
            }
            return radius;
        }

        void CapsuleColliderComponent::SetHeight(float value)
        {
            height = std::max(value, radius * 2.0f);

            if (capsuleCollider) {
                capsuleCollider->SetHeight(height);
            }

            SyncCapsuleShape(bulletObject, capsuleCollider.get(), owner);
            SyncCapsuleTransform(bulletObject, owner, centerOffset);
        }

        float CapsuleColliderComponent::GetHeight() const
        {
            if (capsuleCollider) {
                return capsuleCollider->GetHeight();
            }
            return height;
        }

        void CapsuleColliderComponent::SetCenterOffset(const Math::Vector3& offset)
        {
            centerOffset = offset;
            if (capsuleCollider) {
                capsuleCollider->SetCenter(offset);
            }

            SyncCapsuleTransform(bulletObject, owner, centerOffset);
        }

        Math::Vector3 CapsuleColliderComponent::GetCenterOffset() const
        {
            if (capsuleCollider) {
                return capsuleCollider->GetCenter();
            }
            return centerOffset;
        }

        void CapsuleColliderComponent::SetIsTrigger(bool trigger)
        {
            isTrigger = trigger;
            if (!bulletObject) {
                return;
            }

            int flags = bulletObject->getCollisionFlags();
            if (trigger) {
                flags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
            } else {
                flags &= ~btCollisionObject::CF_NO_CONTACT_RESPONSE;
            }
            bulletObject->setCollisionFlags(flags);
        }

    }
}
