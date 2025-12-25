#pragma once

#include <btBulletDynamicsCommon.h>
#include <memory>
#include "../Math/Vectors/Vector3.h"

namespace RTBEngine {
    namespace Physics {

        class PhysicsWorld {
        public:
            PhysicsWorld();
            ~PhysicsWorld();

            void Initialize();
            void Step(float deltaTime);
            void Cleanup();

            void AddRigidBody(btRigidBody* body);
            void RemoveRigidBody(btRigidBody* body);

            // For static colliders without RigidBody
            void AddCollisionObject(btCollisionObject* obj);
            void RemoveCollisionObject(btCollisionObject* obj);

            void SetGravity(const Math::Vector3& gravity);
            Math::Vector3 GetGravity() const;

            btDynamicsWorld* GetDynamicsWorld() const { return dynamicsWorld.get(); }
            btDispatcher* GetDispatcher() const { return dispatcher.get(); }

        private:
            std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration;
            std::unique_ptr<btCollisionDispatcher> dispatcher;
            std::unique_ptr<btBroadphaseInterface> broadphase;
            std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
            std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld;
        };

    }
}
