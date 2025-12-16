#pragma once

#include "Collider.h"

namespace RTBEngine {
    namespace Physics {

        class SphereCollider : public Collider {
        public:
            SphereCollider();
            SphereCollider(float radius);
            ~SphereCollider() override;

            void SetRadius(float radius);
            float GetRadius() const { return sphereRadius; }

        private:
            void UpdateShape();

            float sphereRadius;
        };

    } 
}
