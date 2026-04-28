#include "BoxColliderComponent.h"
#include "../Physics/BoxCollider.h"
#include "../Physics/PhysicsWorld.h"
#include "RigidBodyComponent.h"
#include "SphereColliderComponent.h"
#include "CapsuleColliderComponent.h"
#include "GameObject.h"

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
