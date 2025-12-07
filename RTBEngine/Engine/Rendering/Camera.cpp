#include "Camera.h"
#include <cmath>

namespace RTBEngine {
    namespace Rendering {

        Camera::Camera()
            : position(Math::Vector3::Zero())
            , forward(Math::Vector3::Forward())
            , right(Math::Vector3::Right())
            , up(Math::Vector3::Up())
            , worldUp(Math::Vector3::Up())
            , pitch(0.0f)
            , yaw(0.0f)
            , fov(60.0f)
            , aspectRatio(16.0f / 9.0f)
            , nearPlane(0.1f)
            , farPlane(1000.0f)
            , orthographicSize(10.0f)
            , projectionType(ProjectionType::Perspective)
            , viewDirty(true)
            , projectionDirty(true)
        {
            UpdateVectors();
        }

        Camera::Camera(const Math::Vector3& position, float fov, float aspectRatio, float nearPlane, float farPlane)
            : position(position)
            , forward(Math::Vector3::Forward())
            , right(Math::Vector3::Right())
            , up(Math::Vector3::Up())
            , worldUp(Math::Vector3::Up())
            , pitch(0.0f)
            , yaw(0.0f)
            , fov(fov)
            , aspectRatio(aspectRatio)
            , nearPlane(nearPlane)
            , farPlane(farPlane)
            , orthographicSize(10.0f)
            , projectionType(ProjectionType::Perspective)
            , viewDirty(true)
            , projectionDirty(true)
        {
            UpdateVectors();
        }

        void Camera::SetPosition(const Math::Vector3& position) {
            this->position = position;
            viewDirty = true;
        }

        void Camera::SetRotation(float pitch, float yaw) {
            this->pitch = pitch;
            this->yaw = yaw;

            if (this->pitch > 89.0f) this->pitch = 89.0f;
            if (this->pitch < -89.0f) this->pitch = -89.0f;

            UpdateVectors();
            viewDirty = true;
        }

        void Camera::SetFOV(float fov) {
            this->fov = fov;
            projectionDirty = true;
        }

        void Camera::SetAspectRatio(float aspectRatio) {
            this->aspectRatio = aspectRatio;
            projectionDirty = true;
        }

        void Camera::SetNearPlane(float nearPlane) {
            this->nearPlane = nearPlane;
            projectionDirty = true;
        }

        void Camera::SetFarPlane(float farPlane) {
            this->farPlane = farPlane;
            projectionDirty = true;
        }

        void Camera::SetProjectionType(ProjectionType type) {
            this->projectionType = type;
            projectionDirty = true;
        }

        void Camera::SetOrthographicSize(float size) {
            this->orthographicSize = size;
            projectionDirty = true;
        }

        Math::Vector3 Camera::GetForward() const {
            return forward;
        }

        Math::Vector3 Camera::GetRight() const {
            return right;
        }

        Math::Vector3 Camera::GetUp() const {
            return up;
        }

        const Math::Matrix4& Camera::GetViewMatrix() {
            if (viewDirty) {
                UpdateViewMatrix();
                viewDirty = false;
            }
            return viewMatrix;
        }

        const Math::Matrix4& Camera::GetProjectionMatrix() {
            if (projectionDirty) {
                UpdateProjectionMatrix();
                projectionDirty = false;
            }
            return projectionMatrix;
        }

        Math::Matrix4 Camera::GetViewProjectionMatrix() {
            return GetProjectionMatrix() * GetViewMatrix();
        }

        void Camera::Move(const Math::Vector3& offset) {
            position += offset;
            viewDirty = true;
        }

        void Camera::MoveForward(float amount) {
            position += forward * amount;
            viewDirty = true;
        }

        void Camera::MoveRight(float amount) {
            position += right * amount;
            viewDirty = true;
        }

        void Camera::MoveUp(float amount) {
            position += worldUp * amount;
            viewDirty = true;
        }

        void Camera::Rotate(float pitchDelta, float yawDelta) {
            pitch += pitchDelta;
            yaw += yawDelta;

            if (pitch > 89.0f) pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;

            UpdateVectors();
            viewDirty = true;
        }

        void Camera::UpdateVectors() {
            const float toRadians = 3.14159265f / 180.0f;

            float pitchRad = pitch * toRadians;
            float yawRad = yaw * toRadians;

            forward.x = cos(pitchRad) * sin(yawRad);
            forward.y = sin(pitchRad);
            forward.z = cos(pitchRad) * cos(yawRad);
            forward.Normalize();

            right = forward.Cross(worldUp).Normalized();
            up = right.Cross(forward).Normalized();
        }

        void Camera::UpdateViewMatrix() {
            Math::Vector3 target = position + forward;
            viewMatrix = Math::Matrix4::LookAt(position, target, worldUp);
        }

        void Camera::UpdateProjectionMatrix() {
            const float toRadians = 3.14159265f / 180.0f;

            if (projectionType == ProjectionType::Perspective) {
                projectionMatrix = Math::Matrix4::Perspective(
                    fov * toRadians,
                    aspectRatio,
                    nearPlane,
                    farPlane
                );
            }
            else {
                float halfHeight = orthographicSize * 0.5f;
                float halfWidth = halfHeight * aspectRatio;
                projectionMatrix = Math::Matrix4::Orthographic(
                    -halfWidth, halfWidth,
                    -halfHeight, halfHeight,
                    nearPlane, farPlane
                );
            }
        }

    }
}