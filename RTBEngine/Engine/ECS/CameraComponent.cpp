#include "CameraComponent.h"
#include "GameObject.h"

namespace RTBEngine {
    namespace ECS {

        CameraComponent::CameraComponent() {
            camera = std::make_unique<Rendering::Camera>(
                Math::Vector3(0.0f, 0.0f, 0.0f),
                45.0f,
                16.0f / 9.0f,
                0.1f,
                100.0f
            );
        }

        CameraComponent::~CameraComponent() = default;

        void CameraComponent::OnUpdate(float deltaTime) {
            if (syncWithTransform) {
                SyncWithTransform();
            }
        }

        void CameraComponent::SyncWithTransform() {
            if (!camera || !owner) return;

            // Sync position
            camera->SetPosition(owner->GetTransform().GetPosition());

            // Sync orientation vectors directly from Transform
            // This ensures Camera uses the corrected GetForward()/GetRight()/GetUp()
            camera->SetDirectionVectors(
                owner->GetTransform().GetForward(),
                owner->GetTransform().GetRight(),
                owner->GetTransform().GetUp()
            );
        }

        void CameraComponent::SetFOV(float fov) {
            if (camera) camera->SetFOV(fov);
        }

        float CameraComponent::GetFOV() const {
            return camera ? camera->GetFOV() : 45.0f;
        }

        void CameraComponent::SetNearPlane(float nearPlane) {
            if (camera) camera->SetNearPlane(nearPlane);
        }

        float CameraComponent::GetNearPlane() const {
            return camera ? camera->GetNearPlane() : 0.1f;
        }

        void CameraComponent::SetFarPlane(float farPlane) {
            if (camera) camera->SetFarPlane(farPlane);
        }

        float CameraComponent::GetFarPlane() const {
            return camera ? camera->GetFarPlane() : 100.0f;
        }

        void CameraComponent::SetProjectionType(Rendering::ProjectionType type) {
            if (camera) camera->SetProjectionType(type);
        }

        Rendering::ProjectionType CameraComponent::GetProjectionType() const {
            return camera ? camera->GetProjectionType() : Rendering::ProjectionType::Perspective;
        }

        void CameraComponent::SetOrthographicSize(float size) {
            if (camera) camera->SetOrthographicSize(size);
        }

        float CameraComponent::GetOrthographicSize() const {
            return camera ? camera->GetOrthographicSize() : 5.0f;
        }

        void CameraComponent::SetAspectRatio(float aspectRatio) {
            if (camera) camera->SetAspectRatio(aspectRatio);
        }

    }
}
