#include "RigidBodyComponent.h"
#include "../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace ECS {

        using ThisClass = RigidBodyComponent;
        RTB_REGISTER_COMPONENT(RigidBodyComponent)
            RTB_PROPERTY(mass)
            RTB_PROPERTY(friction)
            RTB_PROPERTY(restitution)
            RTB_PROPERTY_ENUM(bodyType, "Static", "Dynamic", "Kinematic")
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
            // Create the RigidBody on first wake if it wasn't set by SceneLoader/ConfigureRigidBody.
            // This happens when a component is instantiated via Prefab::Instantiate (copy-paste / prefab drop).
            if (!rigidBody) {
                auto rb = std::make_unique<Physics::RigidBody>();
                rb->SetMass(mass);
                rb->SetFriction(friction);
                rb->SetRestitution(restitution);
                rb->SetType(bodyType);
                rigidBody = std::move(rb);
            }
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

        void RigidBodyComponent::OnValidate()
        {
            SyncProperties();
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
                rigidBody->SetType(bodyType);
            }
        }

    }
}