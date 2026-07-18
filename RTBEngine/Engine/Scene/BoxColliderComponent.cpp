#include "BoxColliderComponent.h"
#include "../Physics/BoxCollider.h"
#include "../Physics/PhysicsWorld.h"
#include "RigidBodyComponent.h"
#include "SphereColliderComponent.h"
#include "CapsuleColliderComponent.h"
#include "MeshRenderer.h"
#include "GameObject.h"
#include <algorithm>
#include <cmath>

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

	void ClearSiblingDynamicColliderRefs(RTBEngine::Scene::GameObject* owner, btRigidBody* bulletBody)
	{
		if (!owner || !bulletBody) {
			return;
		}

		ClearDynamicColliderRef(owner->GetComponent<RTBEngine::Scene::BoxColliderComponent>(), bulletBody);
		ClearDynamicColliderRef(owner->GetComponent<RTBEngine::Scene::SphereColliderComponent>(), bulletBody);
		ClearDynamicColliderRef(owner->GetComponent<RTBEngine::Scene::CapsuleColliderComponent>(), bulletBody);
	}
}

namespace RTBEngine {
	namespace Scene {

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

		void BoxColliderComponent::SetSize(const Math::Vector3& newSize)
		{
			this->size = newSize;
			if (boxCollider) {
				boxCollider->SetSize(newSize);
			}
		}

		Math::Vector3 BoxColliderComponent::GetSize() const
		{
			return size;
		}

		Math::Vector3 BoxColliderComponent::GetCenterOffset() const
		{
			if (boxCollider) {
				return boxCollider->GetCenter();
			}

			return Math::Vector3(0.0f, 0.0f, 0.0f);
		}

		void BoxColliderComponent::FitToOwnerMesh()
		{
			if (!owner || !boxCollider) {
				return;
			}

			MeshRenderer* meshRenderer = owner->GetComponent<MeshRenderer>();
			if (!meshRenderer) {
				return;
			}

			if (!meshRenderer->IsMultiMesh()) {
				Rendering::Mesh* mesh = meshRenderer->GetMesh();
				if (!mesh) {
					return;
				}

				boxCollider->FitToMesh(mesh);
			} else {
				Math::Vector3 localMin;
				Math::Vector3 localMax;
				meshRenderer->GetCombinedAABB(localMin, localMax);
				if (localMin == localMax) {
					return;
				}

				boxCollider->FitToLocalBounds(localMin, localMax);
			}

			const Math::Vector3 objectScale = owner->GetTransform().GetScale();
			const Math::Vector3 fittedSize = boxCollider->GetSize();
			const Math::Vector3 fittedCenter = boxCollider->GetCenter();

			SetSize(Math::Vector3(
				std::max(std::abs(fittedSize.x * objectScale.x), 0.001f),
				std::max(std::abs(fittedSize.y * objectScale.y), 0.001f),
				std::max(std::abs(fittedSize.z * objectScale.z), 0.001f)));
			boxCollider->SetCenter(Math::Vector3(
				fittedCenter.x * objectScale.x,
				fittedCenter.y * objectScale.y,
				fittedCenter.z * objectScale.z));
		}

		void BoxColliderComponent::SetIsTrigger(bool trigger)
		{
			isTrigger = trigger;
		}

	}
}
