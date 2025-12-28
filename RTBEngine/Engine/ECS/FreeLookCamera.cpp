#include "FreeLookCamera.h"
#include "GameObject.h"
#include "../Input/InputManager.h"
#include "../Input/KeyCode.h"
#include "../Input/MouseButton.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace RTBEngine {
    namespace ECS {

        FreeLookCamera::FreeLookCamera()
            : Component()
        {
        }

        void FreeLookCamera::OnStart()
        {
            // Get initial rotation from transform
            if (owner) {
                Math::Vector3 euler = owner->GetTransform().GetRotation().ToEulerAngles();
                pitch = euler.x * (180.0f / 3.14159265f);
                yaw = euler.y * (180.0f / 3.14159265f);
            }
        }

        void FreeLookCamera::OnUpdate(float deltaTime)
        {
            if (!owner) return;

            Input::InputManager& input = Input::InputManager::GetInstance();
            Transform& transform = owner->GetTransform();

            // Mouse look (only when right mouse button is held)
            if (input.IsMouseButtonPressed(Input::MouseButton::Right)) {
                float deltaX = static_cast<float>(input.GetMouseDeltaX());
                float deltaY = static_cast<float>(input.GetMouseDeltaY());

                // Horizontal rotation (yaw around world Y-axis)
                yaw += deltaX * lookSpeed;

                // Vertical rotation (pitch around local X-axis)
                pitch -= deltaY * lookSpeed;

                // Clamp pitch to prevent gimbal lock
                pitch = std::max(-89.0f, std::min(89.0f, pitch));

                // Apply rotation using right-handed YXZ Euler convention
                const float toRadians = 3.14159265f / 180.0f;
                transform.SetRotation(Math::Quaternion::FromEulerAngles(
                    pitch * toRadians,
                    yaw * toRadians,
                    0.0f
                ));
            }

            // Get camera orientation vectors
            Math::Vector3 forward = transform.GetForward();
            Math::Vector3 right = transform.GetRight();
            Math::Vector3 up = transform.GetUp();

            // Debug: Print vectors with P key
            static bool debugPrinted = false;
            if (input.IsKeyJustPressed(Input::KeyCode::P) && !debugPrinted) {
                std::cout << "\n=== FreeLookCamera Debug ===" << std::endl;
                std::cout << "Yaw: " << yaw << "°, Pitch: " << pitch << "°" << std::endl;
                std::cout << "Forward: (" << forward.x << ", " << forward.y << ", " << forward.z << ")" << std::endl;
                std::cout << "Right:   (" << right.x << ", " << right.y << ", " << right.z << ")" << std::endl;
                std::cout << "Up:      (" << up.x << ", " << up.y << ", " << up.z << ")" << std::endl;
                Math::Vector3 pos = transform.GetPosition();
                std::cout << "Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                std::cout << "============================\n" << std::endl;
                debugPrinted = true;
            }
            if (!input.IsKeyPressed(Input::KeyCode::P)) {
                debugPrinted = false;
            }

            // Movement input
            Math::Vector3 movement(0, 0, 0);
            float currentSpeed = moveSpeed;

            // Speed boost with shift
            if (input.IsKeyPressed(Input::KeyCode::LeftShift)) {
                currentSpeed *= 3.0f;
            }

            // W/S - Forward/Backward along camera forward direction
            if (input.IsKeyPressed(Input::KeyCode::W)) {
                movement = movement + forward;
            }
            if (input.IsKeyPressed(Input::KeyCode::S)) {
                movement = movement - forward;
            }

            // A/D - Strafe left/right along camera right direction
            if (input.IsKeyPressed(Input::KeyCode::A)) {
                movement = movement - right;
            }
            if (input.IsKeyPressed(Input::KeyCode::D)) {
                movement = movement + right;
            }

            // Q/E - Rotate around world vertical axis (Y-axis)
            if (input.IsKeyPressed(Input::KeyCode::Q)) {
                yaw -= rotationSpeed * deltaTime;
            }
            if (input.IsKeyPressed(Input::KeyCode::E)) {
                yaw += rotationSpeed * deltaTime;
            }

            // Space/Ctrl - Up/Down along world Y-axis
            if (input.IsKeyPressed(Input::KeyCode::Space)) {
                movement.y += 1.0f;
            }
            if (input.IsKeyPressed(Input::KeyCode::LeftControl)) {
                movement.y -= 1.0f;
            }

            // Apply movement
            if (movement.x != 0.0f || movement.y != 0.0f || movement.z != 0.0f) {
                movement = movement.Normalized() * currentSpeed * deltaTime;
                transform.SetPosition(transform.GetPosition() + movement);
            }

            // Update rotation if Q/E were pressed
            if (input.IsKeyPressed(Input::KeyCode::Q) || input.IsKeyPressed(Input::KeyCode::E)) {
                const float toRadians = 3.14159265f / 180.0f;
                transform.SetRotation(Math::Quaternion::FromEulerAngles(
                    pitch * toRadians,
                    yaw * toRadians,
                    0.0f
                ));
            }
        }

    }
}
