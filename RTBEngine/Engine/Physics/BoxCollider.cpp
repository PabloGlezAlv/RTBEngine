#include "BoxCollider.h"
#include "../Rendering/Mesh.h"

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

        BoxCollider::BoxCollider(Rendering::Mesh* mesh)
            : Collider(ColliderType::Box)
            , boxSize(1.0f, 1.0f, 1.0f) {
            FitToMesh(mesh);
        }

        BoxCollider::~BoxCollider() {
        }

        void BoxCollider::SetSize(const Math::Vector3& size) {
            boxSize = size;
            UpdateShape();
        }

        void BoxCollider::FitToMesh(Rendering::Mesh* mesh) {
            if (mesh) {
                FitToLocalBounds(mesh->GetAABBMin(), mesh->GetAABBMax());
                return;
            }

            UpdateShape();
        }

        void BoxCollider::FitToLocalBounds(const Math::Vector3& localMin, const Math::Vector3& localMax) {
            const Math::Vector3 extent = localMax - localMin;
            boxSize = Math::Vector3(
                std::max(std::abs(extent.x), 0.001f),
                std::max(std::abs(extent.y), 0.001f),
                std::max(std::abs(extent.z), 0.001f));
            SetCenter((localMin + localMax) * 0.5f);
            UpdateShape();
        }

        void BoxCollider::UpdateShape() {
            btVector3 halfExtents(boxSize.x * 0.5f, boxSize.y * 0.5f, boxSize.z * 0.5f);
            SetCollisionShape(new btBoxShape(halfExtents));
        }

    }
} 
