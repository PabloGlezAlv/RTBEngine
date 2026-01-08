#pragma once
#include "Component.h"
#include "../Math/Vectors/Vector3.h"
#include "../Reflection/PropertyMacros.h"
#include <memory>

namespace RTBEngine {
	namespace Physics {
		class BoxCollider;
	}
}

class btCollisionObject;

namespace RTBEngine {
	namespace ECS {

		class BoxColliderComponent : public Component {
		public:
			BoxColliderComponent();
			virtual ~BoxColliderComponent();

			// Size
			void SetSize(const Math::Vector3& size);
			Math::Vector3 GetSize() const;

			// Trigger mode
			void SetIsTrigger(bool trigger);
			bool IsTrigger() const { return isTrigger; }

			// Internal - used by PhysicsSystem
			Physics::BoxCollider* GetBoxCollider() const { return boxCollider.get(); }
			void SetBulletCollisionObject(btCollisionObject* obj) { bulletObject = obj; }
			btCollisionObject* GetBulletCollisionObject() const { return bulletObject; }

			// Reflected properties
			Math::Vector3 size = Math::Vector3(1.0f, 1.0f, 1.0f);
			bool isTrigger = false;

			RTB_COMPONENT(BoxColliderComponent)

		private:
			std::unique_ptr<Physics::BoxCollider> boxCollider;
			btCollisionObject* bulletObject = nullptr; // Owned by PhysicsWorld
		};

	}
}
