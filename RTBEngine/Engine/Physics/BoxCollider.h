#pragma once

#include "Collider.h"
#include "../Math/Vectors/Vector3.h"

namespace RTBEngine {
    namespace Rendering {
        class Mesh;
    }

    namespace Physics {

        class BoxCollider : public Collider {
        public:
            BoxCollider();
            BoxCollider(const Math::Vector3& size);
            BoxCollider(Rendering::Mesh* mesh);
            ~BoxCollider() override;

            void SetSize(const Math::Vector3& size);
            Math::Vector3 GetSize() const { return boxSize; }

            void FitToMesh(Rendering::Mesh* mesh);

        private:
            void UpdateShape();

            Math::Vector3 boxSize;
        };

    }
} 