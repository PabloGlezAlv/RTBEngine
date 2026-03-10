#include "BoxColliderComponent.h"
#include "../Physics/BoxCollider.h"
#include "RigidBodyComponent.h"
#include "GameObject.h"

namespace RTBEngine {
	namespace ECS {

		using ThisClass = BoxColliderComponent;
		RTB_REGISTER_COMPONENT(BoxColliderComponent)
			RTB_PROPERTY(size)
			RTB_PROPERTY(isTrigger)
		RTB_END_REGISTER(BoxColliderComponent)

		BoxColliderComponent::BoxColliderComponent()
			: boxCollider(std::make_unique<Physics::BoxCollider>())
		{
		}

		BoxColliderComponent::~BoxColliderComponent()
		{
			// Only delete if this is a static collision object (not owned by RigidBody).
			// Dynamic btRigidBody is owned by RigidBody::bulletRigidBody (unique_ptr).
			if (bulletObject && !btRigidBody::upcast(bulletObject))
				delete bulletObject;
		}

		void BoxColliderComponent::OnDestroy()
		{
			// If a sibling RigidBodyComponent shares the btRigidBody whose collision shape
			// belongs to this collider, clear it now so it does not hold a dangling shape pointer.
			if (owner) {
				RigidBodyComponent* rb = owner->GetComponent<RigidBodyComponent>();
				if (rb && rb->HasRigidBody()) {
					rb->GetRigidBody()->ClearBulletRigidBody();
				}
			}
		}

		void BoxColliderComponent::SetSize(const Math::Vector3& size)
		{
			if (boxCollider) {
				boxCollider->SetSize(size);
			}
		}

		Math::Vector3 BoxColliderComponent::GetSize() const
		{
			if (boxCollider) {
				return boxCollider->GetSize();
			}
			return Math::Vector3(1.0f, 1.0f, 1.0f);
		}

		void BoxColliderComponent::SetIsTrigger(bool trigger)
		{
			isTrigger = trigger;
		}

	}
}
