#pragma once
#include "Component.h"
#include "../Math/Math.h"
#include "../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace ECS {

        class FreeLookCamera : public Component {
        public:
            FreeLookCamera();
            virtual ~FreeLookCamera() = default;

            virtual void OnStart() override;
            virtual void OnUpdate(float deltaTime) override;

            // Settings
            void SetMoveSpeed(float speed) { moveSpeed = speed; }
            void SetLookSpeed(float speed) { lookSpeed = speed; }
            void SetRotationSpeed(float speed) { rotationSpeed = speed; }
            float GetMoveSpeed() const { return moveSpeed; }
            float GetLookSpeed() const { return lookSpeed; }
            float GetRotationSpeed() const { return rotationSpeed; }

            // Reflected properties
            float moveSpeed = 5.0f;
            float lookSpeed = 0.1f;
            float rotationSpeed = 90.0f;  // Degrees per second for Q/E rotation

            RTB_COMPONENT(FreeLookCamera)

        private:
            float yaw = 0.0f;
            float pitch = 0.0f;
        };

    }
}
