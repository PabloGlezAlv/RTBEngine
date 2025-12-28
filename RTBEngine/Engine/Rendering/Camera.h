#pragma once
#include "../Math/Math.h"

namespace RTBEngine {
    namespace Rendering {

        enum class ProjectionType {
            Perspective,
            Orthographic
        };

        class Camera {
        public:
            Camera();
            Camera(const Math::Vector3& position, float fov, float aspectRatio, float nearPlane, float farPlane);

            void SetPosition(const Math::Vector3& position);
            void SetRotation(float pitch, float yaw);
            void SetDirectionVectors(const Math::Vector3& forward, const Math::Vector3& right, const Math::Vector3& up);
            void SetFOV(float fov);
            void SetAspectRatio(float aspectRatio);
            void SetNearPlane(float nearPlane);
            void SetFarPlane(float farPlane);
            void SetProjectionType(ProjectionType type);
            void SetOrthographicSize(float size);

            const Math::Vector3& GetPosition() const { return position; }
            float GetPitch() const { return pitch; }
            float GetYaw() const { return yaw; }
            float GetFOV() const { return fov; }
            float GetAspectRatio() const { return aspectRatio; }
            float GetNearPlane() const { return nearPlane; }
            float GetFarPlane() const { return farPlane; }
            ProjectionType GetProjectionType() const { return projectionType; }
            float GetOrthographicSize() const { return orthographicSize; }

            Math::Vector3 GetForward() const;
            Math::Vector3 GetRight() const;
            Math::Vector3 GetUp() const;

            const Math::Matrix4& GetViewMatrix();
            const Math::Matrix4& GetProjectionMatrix();
            Math::Matrix4 GetViewProjectionMatrix();

            void Move(const Math::Vector3& offset);
            void MoveForward(float amount);
            void MoveRight(float amount);
            void MoveUp(float amount);
            void Rotate(float pitchDelta, float yawDelta);

            void UpdateVectors();

        private:
            void UpdateViewMatrix();
            void UpdateProjectionMatrix();

            Math::Vector3 position;
            Math::Vector3 forward;
            Math::Vector3 right;
            Math::Vector3 up;
            Math::Vector3 worldUp;

            float pitch;
            float yaw;

            float fov;
            float aspectRatio;
            float nearPlane;
            float farPlane;
            float orthographicSize;

            ProjectionType projectionType;

            Math::Matrix4 viewMatrix;
            Math::Matrix4 projectionMatrix;

            bool viewDirty;
            bool projectionDirty;
        };

    }
}