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

                yaw += deltaX * lookSpeed;
                pitch -= deltaY * lookSpeed;

                // Clamp pitch
                pitch = std::max(-89.0f, std::min(89.0f, pitch));

                // Apply rotation
                const float toRadians = 3.14159265f / 180.0f;
                transform.SetRotation(Math::Quaternion::FromEulerAngles(
                    pitch * toRadians,
                    yaw * toRadians,
                    0.0f
                ));
            }

            // Calculate forward and right vectors from current rotation
            const float toRadians = 3.14159265f / 180.0f;
            float yawRad = yaw * toRadians;
            float pitchRad = pitch * toRadians;

            Math::Vector3 forward;
            forward.x = cos(pitchRad) * sin(yawRad);
            forward.y = sin(pitchRad);
            forward.z = cos(pitchRad) * cos(yawRad);
            forward = forward.Normalized();

            Math::Vector3 right = Math::Vector3(0, 1, 0).Cross(forward).Normalized();
            Math::Vector3 up(0, 1, 0);

            // Movement
            Math::Vector3 movement(0, 0, 0);
            float currentSpeed = moveSpeed;

            // Speed boost with shift
            if (input.IsKeyPressed(Input::KeyCode::LeftShift)) {
                currentSpeed *= 3.0f;
            }

            if (input.IsKeyPressed(Input::KeyCode::W)) {
                movement = movement + forward;
            }
            if (input.IsKeyPressed(Input::KeyCode::S)) {
                movement = movement - forward;
            }
            if (input.IsKeyPressed(Input::KeyCode::A)) {
                movement = movement + right;
            }
            if (input.IsKeyPressed(Input::KeyCode::D)) {
                movement = movement - right;
            }
            if (input.IsKeyPressed(Input::KeyCode::E) || input.IsKeyPressed(Input::KeyCode::Space)) {
                movement = movement + up;
            }
            if (input.IsKeyPressed(Input::KeyCode::Q) || input.IsKeyPressed(Input::KeyCode::LeftControl)) {
                movement = movement - up;
            }

            // Apply movement
            if (movement.x != 0 || movement.y != 0 || movement.z != 0) {
                movement = movement.Normalized() * currentSpeed * deltaTime;
                transform.SetPosition(transform.GetPosition() + movement);
            }

            // Print position with P key
            if (input.IsKeyJustPressed(Input::KeyCode::P)) {
                Math::Vector3 pos = transform.GetPosition();
                std::cout << "[FreeLookCamera] Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
            }
        }

    }
}
