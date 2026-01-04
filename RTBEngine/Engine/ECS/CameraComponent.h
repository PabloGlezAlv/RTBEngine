#pragma once
#include "Component.h"
#include "../Reflection/PropertyMacros.h"
#include "../Rendering/Camera.h"
#include <memory>

namespace RTBEngine {
    namespace ECS {

        class CameraComponent : public Component {
        public:
            CameraComponent();
            ~CameraComponent();

            void OnUpdate(float deltaTime) override;

            // Reflected properties
            float fov = 45.0f;
            float nearClip = 0.1f;
            float farClip = 1000.0f;
            Rendering::ProjectionType projectionType = Rendering::ProjectionType::Perspective;
            float orthographicSize = 10.0f;

            // Transform sync control
            bool syncWithTransform = true;
            bool isMainCamera = false;

            // Getters/Setters wrapping members if needed, or removing them if we use direct access. 
            // Keeping them for compatibility might be wise, updating members inside them?
            // For now, let's keep the API but update it in .cpp implementation to use the members.

            Rendering::Camera* GetCamera() const { return camera.get(); }

            void SetFOV(float fov);
            float GetFOV() const { return fov; }

            void SetNearPlane(float nearPlane);
            float GetNearPlane() const { return nearClip; }

            void SetFarPlane(float farPlane);
            float GetFarPlane() const { return farClip; }

            void SetProjectionType(Rendering::ProjectionType type);
            Rendering::ProjectionType GetProjectionType() const { return projectionType; }

            void SetOrthographicSize(float size);
            float GetOrthographicSize() const { return orthographicSize; }

            void SetAspectRatio(float aspectRatio);

            void SetSyncWithTransform(bool sync) { syncWithTransform = sync; }
            bool GetSyncWithTransform() const { return syncWithTransform; }

            void SetAsMain(bool isMain) { isMainCamera = isMain; }
            bool IsMain() const { return isMainCamera; }

            RTB_COMPONENT(CameraComponent)

        private:
            void SyncWithTransform();
            void SyncProperties();

            std::unique_ptr<Rendering::Camera> camera;
        };

    }
}
