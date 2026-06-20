#pragma once
#include "../RTBEngineAPI.h"
#include "PhysicsWorld.h"
#include "../Scene/Scene.h"
#include "../Scene/GameObject.h"
#include "../Scene/RigidBodyComponent.h"
#include "../Scene/BoxColliderComponent.h"
#include "../Scene/SphereColliderComponent.h"
#include "../Scene/CapsuleColliderComponent.h"
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

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API PhysicsSystem {
        public:
            PhysicsSystem(PhysicsWorld* physicsWorld);
            ~PhysicsSystem();

            void Update(ECS::Scene* scene, float deltaTime);
            void SyncRenderTransforms(ECS::Scene* scene, float interpolationAlpha);
            void InitializeCollider(ECS::GameObject* gameObject, ECS::BoxColliderComponent* boxCollider);
            void InitializeCollider(ECS::GameObject* gameObject, ECS::SphereColliderComponent* sphereCollider);
            void InitializeCollider(ECS::GameObject* gameObject, ECS::CapsuleColliderComponent* capsuleCollider);
            void Reset();

        private:
            std::set<CollisionPair> previousCollisions;
            std::set<CollisionPair> currentCollisions;
            PhysicsWorld* physicsWorld;

            void InitializeStaticCollider(ECS::GameObject* gameObject, ECS::BoxColliderComponent* boxCollider);
            void InitializeDynamicBody(ECS::GameObject* gameObject, ECS::BoxColliderComponent* boxCollider, ECS::RigidBodyComponent* rbComp);

            void InitializeStaticCollider(ECS::GameObject* gameObject, ECS::SphereColliderComponent* sphereCollider);
            void InitializeDynamicBody(ECS::GameObject* gameObject, ECS::SphereColliderComponent* sphereCollider, ECS::RigidBodyComponent* rbComp);

            void InitializeStaticCollider(ECS::GameObject* gameObject, ECS::CapsuleColliderComponent* capsuleCollider);
            void InitializeDynamicBody(ECS::GameObject* gameObject, ECS::CapsuleColliderComponent* capsuleCollider, ECS::RigidBodyComponent* rbComp);

            void SyncTransformsToPhysics(ECS::Scene* scene);
            void SyncPhysicsToTransforms(ECS::Scene* scene, float interpolationAlpha);

            void ProcessCollisions();
            void NotifyCallbacks(ECS::GameObject* object, const CollisionInfo& info, bool isTrigger, CollisionState state);
        };
#pragma warning(pop)

    }
}
