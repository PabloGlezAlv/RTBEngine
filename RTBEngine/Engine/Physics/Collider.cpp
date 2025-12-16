#include "Collider.h"

namespace RTBEngine {
    namespace Physics {

        Collider::Collider(ColliderType type)
            : colliderType(type)
            , centerOffset(0.0f, 0.0f, 0.0f)
            , isTrigger(false)
            , collisionShape(nullptr) {
        }

        Collider::~Collider() {
            // Unique_ptr will automatically clean up
        }

        void Collider::SetCenter(const Math::Vector3& center) {
            centerOffset = center;
        }

        void Collider::SetIsTrigger(bool trigger) {
            isTrigger = trigger;
        }

        void Collider::SetCollisionShape(btCollisionShape* shape) {
            collisionShape.reset(shape);
        }

    } // namespace Physics
} // namespace RTBEngine
