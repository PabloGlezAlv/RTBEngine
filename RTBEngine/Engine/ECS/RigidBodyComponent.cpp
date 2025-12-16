#include "RigidBodyComponent.h"

namespace RTBEngine {
    namespace ECS {

        RigidBodyComponent::RigidBodyComponent()
            : Component(), rigidBody(nullptr), collider(nullptr)
        {
        }

        RigidBodyComponent::~RigidBodyComponent()
        {
            
        }

        void RigidBodyComponent::OnAwake()
        {
            
        }

        void RigidBodyComponent::OnStart()
        {
        }

        void RigidBodyComponent::OnUpdate(float deltaTime)
        {
            
        }

        void RigidBodyComponent::OnDestroy()
        {
            
        }

        void RigidBodyComponent::SetRigidBody(std::unique_ptr<Physics::RigidBody> rb)
        {
            rigidBody = std::move(rb);
        }

        void RigidBodyComponent::SetCollider(std::unique_ptr<Physics::Collider> col)
        {
            collider = std::move(col);
        }

    }
}