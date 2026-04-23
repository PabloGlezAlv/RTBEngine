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
			if (ownsBulletObject && bulletObject)
				delete bulletObject;
		}

		void BoxColliderComponent::OnDestroy()
		{
			if (owner) {
				RigidBodyComponent* rb = owner->GetComponent<RigidBodyComponent>();
				if (rb && rb->HasRigidBody()) {
					rb->GetRigidBody()->ClearBulletRigidBody();
					bulletObject = nullptr;
					ownsBulletObject = false;
				}
			}
		}

		void BoxColliderComponent::SetBulletCollisionObject(btCollisionObject* obj, bool takeOwnership)
		{
			if (ownsBulletObject && bulletObject && bulletObject != obj) {
				delete bulletObject;
			}

			bulletObject = obj;
			ownsBulletObject = takeOwnership;
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
