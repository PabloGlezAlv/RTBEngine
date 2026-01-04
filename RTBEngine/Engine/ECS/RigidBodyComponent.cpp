#include "RigidBodyComponent.h"
#include "../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace ECS {

        using ThisClass = RigidBodyComponent;
        RTB_REGISTER_COMPONENT(RigidBodyComponent)
            RTB_PROPERTY(mass)
            RTB_PROPERTY(friction)
            RTB_PROPERTY(restitution)
            RTB_PROPERTY_ENUM(type, "Static", "Dynamic", "Kinematic")
        RTB_END_REGISTER(RigidBodyComponent)

        RigidBodyComponent::RigidBodyComponent()
            : Component(), rigidBody(nullptr)
        {
        }

        RigidBodyComponent::~RigidBodyComponent()
        {
        }

        void RigidBodyComponent::OnAwake()
        {
            SyncProperties();
        }

        void RigidBodyComponent::OnStart()
        {
            SyncProperties();
        }

        void RigidBodyComponent::OnUpdate(float deltaTime)
        {
            SyncProperties();
        }

        void RigidBodyComponent::OnDestroy()
        {
        }

        void RigidBodyComponent::SetRigidBody(std::unique_ptr<Physics::RigidBody> rb)
        {
            rigidBody = std::move(rb);
            if (rigidBody) {
                // Read back initial values?
                // Or just enforce members? Enforcing members is safer for Inspector sync.
                SyncProperties();
            }
        }

        void RigidBodyComponent::SyncProperties() {
            if (rigidBody) {
                rigidBody->SetMass(mass);
                rigidBody->SetFriction(friction);
                rigidBody->SetRestitution(restitution);
                rigidBody->SetType(type);
            }
        }

    }
}