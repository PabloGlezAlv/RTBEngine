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
            Scene::GameObject* objectA;
            Scene::GameObject* objectB;
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

            void Update(Scene::Scene* scene, float deltaTime);
            void SyncRenderTransforms(Scene::Scene* scene, float interpolationAlpha);
            void InitializeCollider(Scene::GameObject* gameObject, Scene::BoxColliderComponent* boxCollider);
            void InitializeCollider(Scene::GameObject* gameObject, Scene::SphereColliderComponent* sphereCollider);
            void InitializeCollider(Scene::GameObject* gameObject, Scene::CapsuleColliderComponent* capsuleCollider);
            void Reset();

        private:
            std::set<CollisionPair> previousCollisions;
            std::set<CollisionPair> currentCollisions;
            PhysicsWorld* physicsWorld;

            void InitializeStaticCollider(Scene::GameObject* gameObject, Scene::BoxColliderComponent* boxCollider);
            void InitializeDynamicBody(Scene::GameObject* gameObject, Scene::BoxColliderComponent* boxCollider, Scene::RigidBodyComponent* rbComp);

            void InitializeStaticCollider(Scene::GameObject* gameObject, Scene::SphereColliderComponent* sphereCollider);
            void InitializeDynamicBody(Scene::GameObject* gameObject, Scene::SphereColliderComponent* sphereCollider, Scene::RigidBodyComponent* rbComp);

            void InitializeStaticCollider(Scene::GameObject* gameObject, Scene::CapsuleColliderComponent* capsuleCollider);
            void InitializeDynamicBody(Scene::GameObject* gameObject, Scene::CapsuleColliderComponent* capsuleCollider, Scene::RigidBodyComponent* rbComp);

            void SyncTransformsToPhysics(Scene::Scene* scene);
            void SyncPhysicsToTransforms(Scene::Scene* scene, float interpolationAlpha);

            void ProcessCollisions();
            void NotifyCallbacks(Scene::GameObject* object, const CollisionInfo& info, bool isTrigger, CollisionState state);
        };
#pragma warning(pop)

    }
}
