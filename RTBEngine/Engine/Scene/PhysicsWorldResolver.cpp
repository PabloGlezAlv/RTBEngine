#include "PhysicsWorldResolver.h"

#include "BoxColliderComponent.h"
#include "CapsuleColliderComponent.h"
#include "GameObject.h"
#include "RigidBodyComponent.h"
#include "Scene.h"
#include "SphereColliderComponent.h"
#include "../Physics/PhysicsWorld.h"

namespace RTBEngine {
    namespace ECS {

        // Shared helper for NavGridComponent bake and any system that needs a PhysicsWorld pointer.
        // Walks colliders / rigid bodies on the GameObject tree (depth-first when searchChildren).
        Physics::PhysicsWorld* ResolvePhysicsWorldFromGameObject(GameObject* gameObject, bool searchChildren)
        {
            if (!gameObject) {
                return nullptr;
            }

            if (auto* rigidBody = gameObject->GetComponent<RigidBodyComponent>()) {
                if (rigidBody->GetRigidBody() && rigidBody->GetRigidBody()->GetPhysicsWorld()) {
                    return rigidBody->GetRigidBody()->GetPhysicsWorld();
                }
            }

            if (auto* boxCollider = gameObject->GetComponent<BoxColliderComponent>()) {
                if (boxCollider->GetPhysicsWorld()) {
                    return boxCollider->GetPhysicsWorld();
                }
            }

            if (auto* sphereCollider = gameObject->GetComponent<SphereColliderComponent>()) {
                if (sphereCollider->GetPhysicsWorld()) {
                    return sphereCollider->GetPhysicsWorld();
                }
            }

            if (auto* capsuleCollider = gameObject->GetComponent<CapsuleColliderComponent>()) {
                if (capsuleCollider->GetPhysicsWorld()) {
                    return capsuleCollider->GetPhysicsWorld();
                }
            }

            if (!searchChildren) {
                return nullptr;
            }

            for (GameObject* child : gameObject->GetChildren()) {
                if (Physics::PhysicsWorld* world = ResolvePhysicsWorldFromGameObject(child, true)) {
                    return world;
                }
            }

            return nullptr;
        }

        Physics::PhysicsWorld* ResolvePhysicsWorldFromScene(Scene* scene)
        {
            if (!scene) {
                return nullptr;
            }

            for (const auto& gameObject : scene->GetGameObjects()) {
                if (!gameObject) {
                    continue;
                }

                if (Physics::PhysicsWorld* world = ResolvePhysicsWorldFromGameObject(gameObject.get(), true)) {
                    return world;
                }
            }

            return nullptr;
        }

    }
}
