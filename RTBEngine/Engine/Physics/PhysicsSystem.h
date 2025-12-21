#pragma once

#include "PhysicsWorld.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/RigidBodyComponent.h"
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
            void InitializeRigidBody(ECS::GameObject* gameObject, ECS::RigidBodyComponent* component);
            void DestroyRigidBody(ECS::RigidBodyComponent* component);

        private:
            std::set<CollisionPair> previousCollisions;
            std::set<CollisionPair> currentCollisions;

            PhysicsWorld* physicsWorld;


            void SyncTransformsToPhysics(ECS::Scene* scene);
            void SyncPhysicsToTransforms(ECS::Scene* scene);

            void ProcessCollisions();
            void NotifyCallbacks(ECS::GameObject* object, const CollisionInfo& info, bool isTrigger, CollisionState state);
        };

    }
}
