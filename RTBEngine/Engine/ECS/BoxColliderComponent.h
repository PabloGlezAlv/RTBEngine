#pragma once
#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Math/Vectors/Vector3.h"
#include "../Reflection/PropertyMacros.h"
#include <memory>

namespace RTBEngine {
	namespace Physics {
		class BoxCollider;
		class PhysicsWorld;
	}
}

class btCollisionObject;

namespace RTBEngine {
	namespace ECS {

#pragma warning(push)
#pragma warning(disable: 4251)
		class RTB_API BoxColliderComponent : public Component {
		public:
			BoxColliderComponent();
			virtual ~BoxColliderComponent();

			void OnDestroy() override;

			// Size
			void SetSize(const Math::Vector3& size);
			Math::Vector3 GetSize() const;

			// Trigger mode
			void SetIsTrigger(bool trigger);
			bool IsTrigger() const { return isTrigger; }

			// Internal - used by PhysicsSystem
			Physics::BoxCollider* GetBoxCollider() const { return boxCollider.get(); }
			void SetBulletCollisionObject(btCollisionObject* obj, bool takeOwnership = false);
			btCollisionObject* GetBulletCollisionObject() const { return bulletObject; }
			void SetPhysicsWorld(Physics::PhysicsWorld* world) { physicsWorldRef = world; }
			Physics::PhysicsWorld* GetPhysicsWorld() const { return physicsWorldRef; }

			// Reflected properties
			Math::Vector3 size = Math::Vector3(1.0f, 1.0f, 1.0f);
			bool isTrigger = false;

			RTB_COMPONENT(BoxColliderComponent)

		private:
			std::unique_ptr<Physics::BoxCollider> boxCollider;
			btCollisionObject* bulletObject = nullptr;
			bool ownsBulletObject = false;
			Physics::PhysicsWorld* physicsWorldRef = nullptr;
		};
#pragma warning(pop)

	}
}
