#pragma once
#include "../RTBEngineAPI.h"
#include <btBulletDynamicsCommon.h>
#include <memory>
#include "../Math/Vectors/Vector3.h"

namespace RTBEngine {
    namespace ECS {
        class GameObject;
    }

    namespace Physics {

        struct RTB_API PhysicsQueryHit {
            ECS::GameObject* gameObject = nullptr;
            Math::Vector3 point = Math::Vector3(0.0f, 0.0f, 0.0f);
            Math::Vector3 normal = Math::Vector3(0.0f, 1.0f, 0.0f);
            float fraction = 0.0f;
        };

        struct RTB_API PhysicsQueryOptions {
            const ECS::GameObject* ignoredObject = nullptr;
            bool ignoreIgnoredObjectHierarchy = true;
            bool ignoreTriggers = false;
        };

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API PhysicsWorld {
        public:
            PhysicsWorld();
            ~PhysicsWorld();

            void Initialize();
            void Step(float deltaTime);
            void Cleanup();
            void ResetObjects();

            void AddRigidBody(btRigidBody* body);
            void RemoveRigidBody(btRigidBody* body);

            // For static colliders without RigidBody
            void AddCollisionObject(btCollisionObject* obj);
            void RemoveCollisionObject(btCollisionObject* obj);

            void SetGravity(const Math::Vector3& gravity);
            Math::Vector3 GetGravity() const;

            bool RaycastClosest(const Math::Vector3& start,
                                const Math::Vector3& end,
                                PhysicsQueryHit& outHit,
                                const PhysicsQueryOptions& options = PhysicsQueryOptions{}) const;
            bool SphereCastClosest(const Math::Vector3& start,
                                   const Math::Vector3& end,
                                   float radius,
                                   PhysicsQueryHit& outHit,
                                   const PhysicsQueryOptions& options = PhysicsQueryOptions{}) const;

            btDynamicsWorld* GetDynamicsWorld() const { return dynamicsWorld.get(); }
            btDispatcher* GetDispatcher() const { return dispatcher.get(); }
            int GetActiveBodyCount() const { return dynamicsWorld ? dynamicsWorld->getNumCollisionObjects() : 0; }

        private:
            std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration;
            std::unique_ptr<btCollisionDispatcher> dispatcher;
            std::unique_ptr<btBroadphaseInterface> broadphase;
            std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
            std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld;
        };
#pragma warning(pop)

    }
}
