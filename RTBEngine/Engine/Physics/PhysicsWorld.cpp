#include "PhysicsWorld.h"

namespace RTBEngine {
    namespace Physics {

        PhysicsWorld::PhysicsWorld()
            : collisionConfiguration(nullptr)
            , dispatcher(nullptr)
            , broadphase(nullptr)
            , solver(nullptr)
            , dynamicsWorld(nullptr)
        {
        }

        PhysicsWorld::~PhysicsWorld()
        {
            Cleanup();
        }

        void PhysicsWorld::Initialize()
        {
            // Create collision configuration
            collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();

            // Create collision dispatcher
            dispatcher = std::make_unique<btCollisionDispatcher>(collisionConfiguration.get());

            // Create broadphase interface
            broadphase = std::make_unique<btDbvtBroadphase>();

            // Create constraint solver
            solver = std::make_unique<btSequentialImpulseConstraintSolver>();

            // Create dynamics world
            dynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(
                dispatcher.get(),
                broadphase.get(),
                solver.get(),
                collisionConfiguration.get()
            );

            // Set default gravity (9.81 m/s^2 downward)
            dynamicsWorld->setGravity(btVector3(0.0f, -9.81f, 0.0f));
        }

        void PhysicsWorld::Step(float deltaTime)
        {
            if (dynamicsWorld)
            {
                // Step the simulation
                // maxSubSteps = 10, fixedTimeStep = 1/60
                dynamicsWorld->stepSimulation(deltaTime, 10, 1.0f / 60.0f);
            }
        }

        void PhysicsWorld::Cleanup()
        {
            // Remove all rigid bodies from the world
            if (dynamicsWorld)
            {
                for (int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--)
                {
                    btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
                    btRigidBody* body = btRigidBody::upcast(obj);
                    if (body)
                    {
                        dynamicsWorld->removeRigidBody(body);
                    }
                }
            }

            // Clear all unique_ptr members (automatically deletes Bullet objects)
            dynamicsWorld.reset();
            solver.reset();
            broadphase.reset();
            dispatcher.reset();
            collisionConfiguration.reset();
        }

        void PhysicsWorld::AddRigidBody(btRigidBody* body)
        {
            if (dynamicsWorld && body)
            {
                dynamicsWorld->addRigidBody(body);
            }
        }

        void PhysicsWorld::RemoveRigidBody(btRigidBody* body)
        {
            if (dynamicsWorld && body)
            {
                dynamicsWorld->removeRigidBody(body);
            }
        }

        void PhysicsWorld::SetGravity(const Math::Vector3& gravity)
        {
            if (dynamicsWorld)
            {
                dynamicsWorld->setGravity(btVector3(gravity.x, gravity.y, gravity.z));
            }
        }

        Math::Vector3 PhysicsWorld::GetGravity() const
        {
            if (dynamicsWorld)
            {
                btVector3 gravity = dynamicsWorld->getGravity();
                return Math::Vector3(gravity.x(), gravity.y(), gravity.z());
            }
            return Math::Vector3(0.0f, 0.0f, 0.0f);
        }

    }
}
