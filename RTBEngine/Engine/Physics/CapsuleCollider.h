#pragma once
#include "../RTBEngineAPI.h"
#include "Collider.h"

namespace RTBEngine {
    namespace Physics {

        class RTB_API CapsuleCollider : public Collider {
        public:
            CapsuleCollider();
            CapsuleCollider(float radius, float height);
            ~CapsuleCollider() override;

            void SetRadius(float radius);
            float GetRadius() const { return capsuleRadius; }

            void SetHeight(float height);
            float GetHeight() const { return capsuleHeight; }

        private:
            void UpdateShape();

            float capsuleRadius;
            float capsuleHeight;
        };

    }
}
