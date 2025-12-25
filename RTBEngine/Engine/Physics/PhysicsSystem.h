#pragma once

#include "PhysicsWorld.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/RigidBodyComponent.h"
#include "../ECS/BoxColliderComponent.h"
#include <set>

namespace RTBEngine {
    namespace Physics {
        enum class CollisionState {
            Enter,
            Stay,
            Exit
        };

        struct CollisionPair {

            CollisionPair() = default;
            ECS::GameObject* objectA;
            ECS::GameObject* objectB;
            bool isTrigger;

            bool operator<(const CollisionPair& other) const {
                if (objectA != other.objectA) return objectA < other.objectA;
                if (objectB != other.objectB) return objectB < other.objectB;
                return isTrigger < other.isTrigger;
            }

            bool operator==(const CollisionPair& other) const {
                return objectA == other.objectA && objectB == other.objectB && isTrigger == other.isTrigger;
            }
        };

        class PhysicsSystem {
        public:
            PhysicsSystem(PhysicsWorld* physicsWorld);
            ~PhysicsSystem();

            void Update(ECS::Scene* scene, float deltaTime);
            void InitializeCollider(ECS::GameObject* gameObject, ECS::BoxColliderComponent* boxCollider);

        private:
            std::set<CollisionPair> previousCollisions;
            std::set<CollisionPair> currentCollisions;
            PhysicsWorld* physicsWorld;

            void InitializeStaticCollider(ECS::GameObject* gameObject, ECS::BoxColliderComponent* boxCollider);
            void InitializeDynamicBody(ECS::GameObject* gameObject, ECS::BoxColliderComponent* boxCollider, ECS::RigidBodyComponent* rbComp);

            void SyncTransformsToPhysics(ECS::Scene* scene);
            void SyncPhysicsToTransforms(ECS::Scene* scene);

            void ProcessCollisions();
            void NotifyCallbacks(ECS::GameObject* object, const CollisionInfo& info, bool isTrigger, CollisionState state);
        };

    }
}
