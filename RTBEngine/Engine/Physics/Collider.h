#pragma once

#include <btBulletDynamicsCommon.h>
#include <memory>
#include "../Math/Vectors/Vector3.h"

namespace RTBEngine {
    namespace Physics {

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

            btCollisionShape* GetCollisionShape() { return collisionShape.get(); }
            const btCollisionShape* GetCollisionShape() const { return collisionShape.get(); }

        protected:
            void SetCollisionShape(btCollisionShape* shape);

        private:
            ColliderType colliderType;
            Math::Vector3 centerOffset;
            std::unique_ptr<btCollisionShape> collisionShape;
        };

    }
} 
