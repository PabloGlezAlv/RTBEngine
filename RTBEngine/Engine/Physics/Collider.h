#pragma once

#include <btBulletDynamicsCommon.h>
#include <memory>
#include "../Math/Vectors/Vector3.h"

namespace RTBEngine {
    namespace Physics {

        class RigidBody;

        enum class ColliderType {
            Box,
            Sphere,
            Capsule,
            Mesh
        };

        class Collider {
        public:
            Collider(ColliderType type);
            virtual ~Collider();

            ColliderType GetType() const { return colliderType; }

            void SetCenter(const Math::Vector3& center);
            Math::Vector3 GetCenter() const { return centerOffset; }

            void SetIsTrigger(bool trigger);
            bool IsTrigger() const { return isTrigger; }

            btCollisionShape* GetCollisionShape() { return collisionShape.get(); }
            const btCollisionShape* GetCollisionShape() const { return collisionShape.get(); }

            void SetAssociatedRigidBody(RigidBody* rb) { associatedRigidBody = rb; }
            RigidBody* GetAssociatedRigidBody() const { return associatedRigidBody; }

        protected:
            void SetCollisionShape(btCollisionShape* shape);

        private:
            ColliderType colliderType;
            Math::Vector3 centerOffset;
            bool isTrigger;
            std::unique_ptr<btCollisionShape> collisionShape;
            RigidBody* associatedRigidBody = nullptr;
        };

    }
} 
