#pragma once
#include "Component.h"
#include "../Math/Math.h"

namespace RTBEngine {
    namespace ECS {

        class FreeLookCamera : public Component {
        public:
            FreeLookCamera();
            virtual ~FreeLookCamera() = default;

            virtual void OnStart() override;
            virtual void OnUpdate(float deltaTime) override;
            virtual const char* GetTypeName() const override { return "FreeLookCamera"; }

            // Settings
            void SetMoveSpeed(float speed) { moveSpeed = speed; }
            void SetLookSpeed(float speed) { lookSpeed = speed; }
            float GetMoveSpeed() const { return moveSpeed; }
            float GetLookSpeed() const { return lookSpeed; }

        private:
            float moveSpeed = 5.0f;
            float lookSpeed = 0.1f;
            float yaw = 0.0f;
            float pitch = 0.0f;
            bool firstMouse = true;
            float lastMouseX = 0.0f;
            float lastMouseY = 0.0f;
        };

    }
}
