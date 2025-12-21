#pragma once
#include <btBulletDynamicsCommon.h>
#include <memory>

namespace RTBEngine {

    namespace ECS {
        class GameObject;
    }

    namespace Physics {

        enum class RigidBodyType {
            Static,      
            Dynamic,     
            Kinematic    
        };

        class RigidBody {
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

            btRigidBody* GetBulletRigidBody() { return bulletRigidBody.get(); }
            const btRigidBody* GetBulletRigidBody() const { return bulletRigidBody.get(); }

            void SetBulletRigidBody(std::unique_ptr<btRigidBody> rigidBody);

            bool IsInitialized() const { return bulletRigidBody != nullptr; }

            void SetOwner(ECS::GameObject* gameObject) { owner = gameObject; }
            ECS::GameObject* GetOwner() const { return owner; }
        private:
            RigidBodyType bodyType;
            float bodyMass;
            float bodyFriction;
            float bodyRestitution;

            std::unique_ptr<btRigidBody> bulletRigidBody;

            ECS::GameObject* owner = nullptr;
        };

    } 
}
