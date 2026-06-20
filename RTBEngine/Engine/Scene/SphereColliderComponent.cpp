#include "SphereColliderComponent.h"
#include "../Physics/SphereCollider.h"
#include "../Physics/PhysicsWorld.h"
#include "RigidBodyComponent.h"
#include "BoxColliderComponent.h"
#include "CapsuleColliderComponent.h"
#include "GameObject.h"
#include <BulletDynamics/Dynamics/btRigidBody.h>

namespace {
	template <typename ColliderComponent>
	void ClearDynamicColliderRef(ColliderComponent* collider, btRigidBody* bulletBody)
	{
		if (!collider || !bulletBody || collider->GetBulletCollisionObject() != bulletBody) {
			return;
		}

		collider->SetPhysicsWorld(nullptr);
		collider->SetBulletCollisionObject(nullptr, false);
	}

	void ClearSiblingDynamicColliderRefs(RTBEngine::ECS::GameObject* owner, btRigidBody* bulletBody)
	{
		if (!owner || !bulletBody) {
			return;
		}

		ClearDynamicColliderRef(owner->GetComponent<RTBEngine::ECS::BoxColliderComponent>(), bulletBody);
		ClearDynamicColliderRef(owner->GetComponent<RTBEngine::ECS::SphereColliderComponent>(), bulletBody);
		ClearDynamicColliderRef(owner->GetComponent<RTBEngine::ECS::CapsuleColliderComponent>(), bulletBody);
	}
}

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
			if (ownsBulletObject && bulletObject)
				delete bulletObject;
		}

		void SphereColliderComponent::OnDestroy()
		{
			if (owner) {
				RigidBodyComponent* rb = owner->GetComponent<RigidBodyComponent>();
				Physics::RigidBody* rigidBody = (rb && rb->HasRigidBody()) ? rb->GetRigidBody() : nullptr;
				btRigidBody* sharedBody = rigidBody ? rigidBody->GetBulletRigidBody() : nullptr;

				if (sharedBody && bulletObject == sharedBody) {
					if (owner->IsBeingDestroyed()) {
						SetPhysicsWorld(nullptr);
						SetBulletCollisionObject(nullptr, false);
						return;
					}

					ClearSiblingDynamicColliderRefs(owner, sharedBody);
					rigidBody->ClearBulletRigidBody();
					rigidBody->SetPhysicsWorld(nullptr);
					return;
				}
			}

			if (bulletObject) {
				if (Physics::PhysicsWorld* world = GetPhysicsWorld()) {
					world->RemoveCollisionObject(bulletObject);
				}

				SetPhysicsWorld(nullptr);
				SetBulletCollisionObject(nullptr, false);
			}
		}

		void SphereColliderComponent::SetBulletCollisionObject(btCollisionObject* obj, bool takeOwnership)
		{
			if (ownsBulletObject && bulletObject && bulletObject != obj) {
				delete bulletObject;
			}

			bulletObject = obj;
			ownsBulletObject = takeOwnership;
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
