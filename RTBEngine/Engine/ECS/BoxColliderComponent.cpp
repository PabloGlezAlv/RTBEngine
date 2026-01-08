#include "BoxColliderComponent.h"
#include "../Physics/BoxCollider.h"

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
