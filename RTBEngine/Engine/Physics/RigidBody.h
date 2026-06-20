#pragma once
#include "../RTBEngineAPI.h"
#include <btBulletDynamicsCommon.h>
#include "../Math/Vectors/Vector3.h"
#include "../Math/Quaternions/Quaternion.h"
#include <memory>

namespace RTBEngine {

    namespace ECS {
        class GameObject;
    }

    namespace Physics {

        class PhysicsWorld;

        enum class RigidBodyType {
            Static,      
            Dynamic,     
            Kinematic    
        };

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API RigidBody {
        public:
            RigidBody();
            ~RigidBody();

            void SetType(RigidBodyType type);
            RigidBodyType GetType() const { return bodyType; }

            void SetMass(float mass);
            float GetMass() const { return bodyMass; }

            void SetFriction(float friction);
            float GetFriction() const { return bodyFriction; }

            void SetRestitution(float restitution);
            float GetRestitution() const { return bodyRestitution; }

            void SetLinearVelocity(const btVector3& velocity);
            btVector3 GetLinearVelocity() const;

            void SetAngularVelocity(const btVector3& velocity);
            btVector3 GetAngularVelocity() const;

            void ApplyForce(const btVector3& force, const btVector3& relativePos = btVector3(0, 0, 0));
            void ApplyImpulse(const btVector3& impulse, const btVector3& relativePos = btVector3(0, 0, 0));
            void ApplyCentralForce(const btVector3& force);
            void ApplyCentralImpulse(const btVector3& impulse);

            void SetGravity(const btVector3& gravity);
            btVector3 GetGravity() const;

            void SetAngularFactor(const btVector3& factor);
            void SetLinearFactor(const btVector3& factor);
            void SetWorldTransform(const Math::Vector3& position, const Math::Quaternion& rotation);

            btRigidBody* GetBulletRigidBody() { return bulletRigidBody.get(); }
            const btRigidBody* GetBulletRigidBody() const { return bulletRigidBody.get(); }

            void SetBulletRigidBody(std::unique_ptr<btRigidBody> rigidBody);
            void ClearBulletRigidBody();

            bool IsInitialized() const { return bulletRigidBody != nullptr; }

            void SetOwner(ECS::GameObject* gameObject) { owner = gameObject; }
            ECS::GameObject* GetOwner() const { return owner; }

            void SetPhysicsWorld(PhysicsWorld* world) { physicsWorldRef = world; }
            PhysicsWorld* GetPhysicsWorld() const { return physicsWorldRef; }
        private:
            RigidBodyType bodyType;
            float bodyMass;
            float bodyFriction;
            float bodyRestitution;
            btVector3 bodyAngularFactor;
            btVector3 bodyLinearFactor;

            std::unique_ptr<btRigidBody> bulletRigidBody;

            ECS::GameObject* owner = nullptr;
            PhysicsWorld* physicsWorldRef = nullptr;
        };
#pragma warning(pop)

    }
}
