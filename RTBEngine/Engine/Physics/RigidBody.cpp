#include "RigidBody.h"

namespace RTBEngine {
    namespace Physics {

        RigidBody::RigidBody()
            : bodyType(RigidBodyType::Dynamic)
            , bodyMass(1.0f)
            , bodyFriction(0.5f)
            , bodyRestitution(0.0f)
            , bulletRigidBody(nullptr) {
        }

        RigidBody::~RigidBody() {
            
        }

        void RigidBody::SetType(RigidBodyType type) {
            bodyType = type;

            if (bulletRigidBody) {
                if (type == RigidBodyType::Static) {
                    bulletRigidBody->setCollisionFlags(bulletRigidBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
                    bulletRigidBody->setMassProps(0.0f, btVector3(0, 0, 0));
                }
                else if (type == RigidBodyType::Kinematic) {
                    bulletRigidBody->setCollisionFlags(bulletRigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
                    bulletRigidBody->setActivationState(DISABLE_DEACTIVATION);
                }
                else { // Dynamic
                    bulletRigidBody->setCollisionFlags(bulletRigidBody->getCollisionFlags() & ~btCollisionObject::CF_STATIC_OBJECT);
                    bulletRigidBody->setCollisionFlags(bulletRigidBody->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
                }
            }
        }

        void RigidBody::SetMass(float mass) {
            bodyMass = mass;

            if (bulletRigidBody && bulletRigidBody->getCollisionShape()) {
                btVector3 localInertia(0, 0, 0);
                if (mass > 0.0f) {
                    bulletRigidBody->getCollisionShape()->calculateLocalInertia(mass, localInertia);
                }
                bulletRigidBody->setMassProps(mass, localInertia);
            }
        }

        void RigidBody::SetFriction(float friction) {
            bodyFriction = friction;
            if (bulletRigidBody) {
                bulletRigidBody->setFriction(friction);
            }
        }

        void RigidBody::SetRestitution(float restitution) {
            bodyRestitution = restitution;
            if (bulletRigidBody) {
                bulletRigidBody->setRestitution(restitution);
            }
        }

        void RigidBody::SetLinearVelocity(const btVector3& velocity) {
            if (bulletRigidBody) {
                bulletRigidBody->setLinearVelocity(velocity);
                bulletRigidBody->activate();
            }
        }

        btVector3 RigidBody::GetLinearVelocity() const {
            if (bulletRigidBody) {
                return bulletRigidBody->getLinearVelocity();
            }
            return btVector3(0, 0, 0);
        }

        void RigidBody::SetAngularVelocity(const btVector3& velocity) {
            if (bulletRigidBody) {
                bulletRigidBody->setAngularVelocity(velocity);
                bulletRigidBody->activate();
            }
        }

        btVector3 RigidBody::GetAngularVelocity() const {
            if (bulletRigidBody) {
                return bulletRigidBody->getAngularVelocity();
            }
            return btVector3(0, 0, 0);
        }

        void RigidBody::ApplyForce(const btVector3& force, const btVector3& relativePos) {
            if (bulletRigidBody) {
                bulletRigidBody->applyForce(force, relativePos);
                bulletRigidBody->activate();
            }
        }

        void RigidBody::ApplyImpulse(const btVector3& impulse, const btVector3& relativePos) {
            if (bulletRigidBody) {
                bulletRigidBody->applyImpulse(impulse, relativePos);
                bulletRigidBody->activate();
            }
        }

        void RigidBody::ApplyCentralForce(const btVector3& force) {
            if (bulletRigidBody) {
                bulletRigidBody->applyCentralForce(force);
                bulletRigidBody->activate();
            }
        }

        void RigidBody::ApplyCentralImpulse(const btVector3& impulse) {
            if (bulletRigidBody) {
                bulletRigidBody->applyCentralImpulse(impulse);
                bulletRigidBody->activate();
            }
        }

        void RigidBody::SetGravity(const btVector3& gravity) {
            if (bulletRigidBody) {
                bulletRigidBody->setGravity(gravity);
            }
        }

        btVector3 RigidBody::GetGravity() const {
            if (bulletRigidBody) {
                return bulletRigidBody->getGravity();
            }
            return btVector3(0, 0, 0);
        }

        void RigidBody::SetAngularFactor(const btVector3& factor) {
            if (bulletRigidBody) {
                bulletRigidBody->setAngularFactor(factor);
            }
        }

        void RigidBody::SetLinearFactor(const btVector3& factor) {
            if (bulletRigidBody) {
                bulletRigidBody->setLinearFactor(factor);
            }
        }

        void RigidBody::SetBulletRigidBody(std::unique_ptr<btRigidBody> rigidBody) {
            bulletRigidBody = std::move(rigidBody);

            if (bulletRigidBody) {
                SetFriction(bodyFriction);
                SetRestitution(bodyRestitution);
                SetType(m_type);
            }
        }

    } 
}
