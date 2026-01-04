#pragma once
#include "Component.h"
#include "../Reflection/PropertyMacros.h"
#include "../Physics/RigidBody.h"
#include <memory>

namespace RTBEngine {
    namespace ECS {

        class RigidBodyComponent : public Component {
        public:
            RigidBodyComponent();
            ~RigidBodyComponent();

            void OnAwake() override;
            void OnStart() override;
            void OnUpdate(float deltaTime) override;
            void OnDestroy() override;

            void SetRigidBody(std::unique_ptr<Physics::RigidBody> rb);
            Physics::RigidBody* GetRigidBody() const { return rigidBody.get(); }

            bool HasRigidBody() const { return rigidBody != nullptr; }

            // Reflected properties
            float mass = 1.0f;
            float friction = 0.5f;
            float restitution = 0.0f;
            Physics::RigidBodyType type = Physics::RigidBodyType::Dynamic;

            RTB_COMPONENT(RigidBodyComponent)

        private:
            void SyncProperties();
            std::unique_ptr<Physics::RigidBody> rigidBody;
        };

    }
}