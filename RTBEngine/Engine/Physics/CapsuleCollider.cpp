#include "CapsuleCollider.h"

#include <algorithm>

namespace RTBEngine {
    namespace Physics {

        CapsuleCollider::CapsuleCollider()
            : Collider(ColliderType::Capsule)
            , capsuleRadius(0.5f)
            , capsuleHeight(2.0f) {
            UpdateShape();
        }

        CapsuleCollider::CapsuleCollider(float radius, float height)
            : Collider(ColliderType::Capsule)
            , capsuleRadius(radius)
            , capsuleHeight(height) {
            UpdateShape();
        }

        CapsuleCollider::~CapsuleCollider() {
        }

        void CapsuleCollider::SetRadius(float radius) {
            capsuleRadius = std::max(0.01f, radius);
            capsuleHeight = std::max(capsuleHeight, capsuleRadius * 2.0f);
            UpdateShape();
        }

        void CapsuleCollider::SetHeight(float height) {
            capsuleHeight = std::max(height, capsuleRadius * 2.0f);
            UpdateShape();
        }

        void CapsuleCollider::UpdateShape() {
            const float clampedRadius = std::max(0.01f, capsuleRadius);
            const float clampedHeight = std::max(capsuleHeight, clampedRadius * 2.0f);
            const float cylinderHeight = std::max(0.0f, clampedHeight - clampedRadius * 2.0f);
            SetCollisionShape(new btCapsuleShape(clampedRadius, cylinderHeight));
        }

    }
}
