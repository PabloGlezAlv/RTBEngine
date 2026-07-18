#pragma once
#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Math/Vectors/Vector3.h"
#include "../Reflection/PropertyMacros.h"
#include <memory>

namespace RTBEngine {
	namespace Physics {
		class SphereCollider;
		class PhysicsWorld;
	}
}

class btCollisionObject;

namespace RTBEngine {
	namespace Scene {

#pragma warning(push)
#pragma warning(disable: 4251)
		class RTB_API SphereColliderComponent : public Component {
		public:
			SphereColliderComponent();
			virtual ~SphereColliderComponent();

			SphereColliderComponent(const SphereColliderComponent&) = delete;
			SphereColliderComponent& operator=(const SphereColliderComponent&) = delete;

			void OnDestroy() override;

			//Radius
			void SetRadius(float r);
			float GetRadius() const;

			//Center offset
			void SetCenterOffset(const Math::Vector3& offset);
			Math::Vector3 GetCenterOffset() const;

			//Trigger mode
			void SetIsTrigger(bool trigger) { isTrigger = trigger; }
			bool IsTrigger() const { return isTrigger; }

			//Internal - used by PhysicsSystem
			Physics::SphereCollider* GetSphereCollider() const { return sphereCollider.get(); }
			void SetBulletCollisionObject(btCollisionObject* obj, bool takeOwnership = false);
			btCollisionObject* GetBulletCollisionObject() const { return bulletObject; }
			void SetPhysicsWorld(Physics::PhysicsWorld* world) { physicsWorldRef = world; }
			Physics::PhysicsWorld* GetPhysicsWorld() const { return physicsWorldRef; }

			// Reflected properties (Proxy)
			float radius = 0.5f;
			Math::Vector3 centerOffset;
			bool isTrigger = false;

			RTB_COMPONENT(SphereColliderComponent)

		private:
			std::unique_ptr<Physics::SphereCollider> sphereCollider;
			btCollisionObject* bulletObject = nullptr;
			bool ownsBulletObject = false;
			Physics::PhysicsWorld* physicsWorldRef = nullptr;
		};
#pragma warning(pop)

	}
}
