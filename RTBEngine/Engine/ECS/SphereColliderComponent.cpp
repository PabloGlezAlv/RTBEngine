#include "SphereColliderComponent.h"
#include "../Physics/SphereCollider.h"
#include "RigidBodyComponent.h"
#include "GameObject.h"
#include <BulletDynamics/Dynamics/btRigidBody.h>

namespace RTBEngine {
	namespace ECS {

		using ThisClass = SphereColliderComponent;
		RTB_REGISTER_COMPONENT(SphereColliderComponent)
			RTB_PROPERTY(radius)
			RTB_PROPERTY(centerOffset)
			RTB_PROPERTY(isTrigger)
		RTB_END_REGISTER(SphereColliderComponent)

		SphereColliderComponent::SphereColliderComponent()
			: sphereCollider(std::make_unique<Physics::SphereCollider>(radius))
		{
		}

		SphereColliderComponent::~SphereColliderComponent()
		{
			// Only delete if this is a static collision object (not owned by RigidBody).
			// Dynamic btRigidBody is owned by RigidBody::bulletRigidBody (unique_ptr).
			if (bulletObject && !btRigidBody::upcast(bulletObject))
				delete bulletObject;
		}

		void SphereColliderComponent::OnDestroy()
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

		void SphereColliderComponent::SetRadius(float r)
		{
			radius = r;
			if (sphereCollider)
				sphereCollider->SetRadius(r);
		}

		float SphereColliderComponent::GetRadius() const
		{
			if (sphereCollider)
				return sphereCollider->GetRadius();
			return 0.5f;
		}

		void SphereColliderComponent::SetCenterOffset(const Math::Vector3& offset)
		{
			centerOffset = offset;
			if (sphereCollider)
				sphereCollider->SetCenter(offset);
		}

		Math::Vector3 SphereColliderComponent::GetCenterOffset() const
		{
			if (sphereCollider)
				return sphereCollider->GetCenter();
			return centerOffset;
		}

	}
}
