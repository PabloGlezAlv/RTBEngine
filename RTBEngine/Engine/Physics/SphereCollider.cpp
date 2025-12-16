#include "SphereCollider.h"

namespace RTBEngine {
    namespace Physics {

        SphereCollider::SphereCollider()
            : Collider(ColliderType::Sphere)
            , sphereRadius(0.5f) {
            UpdateShape();
        }

        SphereCollider::SphereCollider(float radius)
            : Collider(ColliderType::Sphere)
            , sphereRadius(radius) {
            UpdateShape();
        }

        SphereCollider::~SphereCollider() {
        }

        void SphereCollider::SetRadius(float radius) {
            sphereRadius = radius;
            UpdateShape();
        }

        void SphereCollider::UpdateShape() {
            SetCollisionShape(new btSphereShape(sphereRadius));
        }

    } 
} 