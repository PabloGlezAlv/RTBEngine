#include "Collider.h"

namespace RTBEngine {
    namespace Physics {

        Collider::Collider(ColliderType type)
            : colliderType(type)
            , centerOffset(0.0f, 0.0f, 0.0f)
            , collisionShape(nullptr) {
        }

        Collider::~Collider() {
        }

        void Collider::SetCenter(const Math::Vector3& center) {
            centerOffset = center;
        }

        void Collider::SetCollisionShape(btCollisionShape* shape) {
            collisionShape.reset(shape);
        }

    }
} 
