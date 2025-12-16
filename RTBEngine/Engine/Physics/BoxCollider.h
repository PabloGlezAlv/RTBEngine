#pragma once

#include "Collider.h"
#include "../Math/Vectors/Vector3.h"

namespace RTBEngine {
    namespace Physics {

        class BoxCollider : public Collider {
        public:
            BoxCollider();
            BoxCollider(const Math::Vector3& size);
            ~BoxCollider() override;

            void SetSize(const Math::Vector3& size);
            Math::Vector3 GetSize() const { return boxSize; }

        private:
            void UpdateShape();

            Math::Vector3 boxSize;
        };

    } // namespace Physics
} // namespace RTBEngine
