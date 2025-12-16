#include "BoxCollider.h"

namespace RTBEngine {
    namespace Physics {

        BoxCollider::BoxCollider()
            : Collider(ColliderType::Box)
            , boxSize(1.0f, 1.0f, 1.0f) {
            UpdateShape();
        }

        BoxCollider::BoxCollider(const Math::Vector3& size)
            : Collider(ColliderType::Box)
            , boxSize(size) {
            UpdateShape();
        }

        BoxCollider::~BoxCollider() {
        }

        void BoxCollider::SetSize(const Math::Vector3& size) {
            boxSize = size;
            UpdateShape();
        }

        void BoxCollider::UpdateShape() {

            btVector3 halfExtents(boxSize.x * 0.5f, boxSize.y * 0.5f, boxSize.z * 0.5f);
            SetCollisionShape(new btBoxShape(halfExtents));
        }

    } 
} 
