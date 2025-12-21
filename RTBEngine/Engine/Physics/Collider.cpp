#include "Collider.h"
#include "RigidBody.h"

namespace RTBEngine {
    namespace Physics {

        Collider::Collider(ColliderType type)
            : colliderType(type)
            , centerOffset(0.0f, 0.0f, 0.0f)
            , isTrigger(false)
            , collisionShape(nullptr)
            , associatedRigidBody(nullptr) {
        }

        Collider::~Collider() {

        }

        void Collider::SetCenter(const Math::Vector3& center) {
            centerOffset = center;
        }

        void Collider::SetIsTrigger(bool trigger) {
            isTrigger = trigger;

            // Update btRigidBody flags if already initialized
            if (associatedRigidBody && associatedRigidBody->GetBulletRigidBody()) {
                btRigidBody* btBody = associatedRigidBody->GetBulletRigidBody();
                if (trigger) {
                    btBody->setCollisionFlags(btBody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
                } else {
                    btBody->setCollisionFlags(btBody->getCollisionFlags() & ~btCollisionObject::CF_NO_CONTACT_RESPONSE);
                }
            }
        }

        void Collider::SetCollisionShape(btCollisionShape* shape) {
            collisionShape.reset(shape);
        }

    }
} 
