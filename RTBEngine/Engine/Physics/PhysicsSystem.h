#pragma once

#include "PhysicsWorld.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/RigidBodyComponent.h"

namespace RTBEngine {
    namespace Physics {

        class PhysicsSystem {
        public:
            PhysicsSystem(PhysicsWorld* physicsWorld);
            ~PhysicsSystem();

            void Update(ECS::Scene* scene, float deltaTime);
            void InitializeRigidBody(ECS::GameObject* gameObject, ECS::RigidBodyComponent* component);
            void DestroyRigidBody(ECS::RigidBodyComponent* component);

        private:
            void SyncTransformsToPhysics(ECS::Scene* scene);
            void SyncPhysicsToTransforms(ECS::Scene* scene);

            PhysicsWorld* physicsWorld;
        };

    }
}
