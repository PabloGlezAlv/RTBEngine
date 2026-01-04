#include "CameraComponent.h"
#include "GameObject.h"
#include "../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace ECS {

        using ThisClass = CameraComponent;
        RTB_REGISTER_COMPONENT(CameraComponent)
            RTB_PROPERTY(fov)
            RTB_PROPERTY(nearClip)
            RTB_PROPERTY(farClip)
            RTB_PROPERTY_ENUM(projectionType, "Perspective", "Orthographic")
            RTB_PROPERTY(orthographicSize)
            RTB_PROPERTY(syncWithTransform)
            RTB_PROPERTY(isMainCamera)
        RTB_END_REGISTER(CameraComponent)

        CameraComponent::CameraComponent() {
            camera = std::make_unique<Rendering::Camera>(
                Math::Vector3(0.0f, 0.0f, 0.0f),
                fov,
                16.0f / 9.0f,
                nearClip,
                farClip
            );
        }

        CameraComponent::~CameraComponent() = default;

        void CameraComponent::OnUpdate(float deltaTime) {
            SyncProperties();
            if (syncWithTransform) {
                SyncWithTransform();
            }
        }

        void CameraComponent::SyncProperties() {
            if (!camera) return;
            camera->SetFOV(fov);
            camera->SetNearPlane(nearClip);
            camera->SetFarPlane(farClip);
            camera->SetProjectionType(projectionType);
            camera->SetOrthographicSize(orthographicSize);
        }

        void CameraComponent::SyncWithTransform() {
            if (!camera || !owner) return;

            // Sync position
            camera->SetPosition(owner->GetWorldPosition());

            // Get World Matrix to extract direction vectors
            Math::Matrix4 wm = owner->GetWorldMatrix();
            
            // Forward (3rd column)
            Math::Vector3 forward(wm[8], wm[9], wm[10]);
            // Right (1st column)
            Math::Vector3 right(-wm[0], -wm[1], -wm[2]); // Match negated yaw convention
            // Up (2nd column)
            Math::Vector3 up(wm[4], wm[5], wm[6]);

            camera->SetDirectionVectors(forward.Normalized(), right.Normalized(), up.Normalized());
        }

        void CameraComponent::SetFOV(float fov) {
            this->fov = fov;
            if (camera) camera->SetFOV(fov);
        }

        void CameraComponent::SetNearPlane(float nearPlane) {
            this->nearClip = nearPlane;
            if (camera) camera->SetNearPlane(nearPlane);
        }

        void CameraComponent::SetFarPlane(float farPlane) {
            this->farClip = farPlane;
            if (camera) camera->SetFarPlane(farPlane);
        }

        void CameraComponent::SetProjectionType(Rendering::ProjectionType type) {
            this->projectionType = type;
            if (camera) camera->SetProjectionType(type);
        }

        void CameraComponent::SetOrthographicSize(float size) {
            this->orthographicSize = size;
            if (camera) camera->SetOrthographicSize(size);
        }

        void CameraComponent::SetAspectRatio(float aspectRatio) {
            if (camera) camera->SetAspectRatio(aspectRatio);
        }

    }
}
