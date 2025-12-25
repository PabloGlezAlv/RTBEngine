#pragma once
#include "Component.h"
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

            const char* GetTypeName() const override { return "RigidBodyComponent"; }

        private:
            std::unique_ptr<Physics::RigidBody> rigidBody;
        };

    }
}