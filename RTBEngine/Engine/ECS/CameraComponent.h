#pragma once
#include "Component.h"
#include "../Rendering/Camera.h"
#include <memory>

namespace RTBEngine {
    namespace ECS {

        class CameraComponent : public Component {
        public:
            CameraComponent();
            ~CameraComponent();

            void OnUpdate(float deltaTime) override;

            Rendering::Camera* GetCamera() const { return camera.get(); }

            void SetFOV(float fov);
            float GetFOV() const;

            void SetNearPlane(float nearPlane);
            float GetNearPlane() const;

            void SetFarPlane(float farPlane);
            float GetFarPlane() const;

            void SetProjectionType(Rendering::ProjectionType type);
            Rendering::ProjectionType GetProjectionType() const;

            void SetOrthographicSize(float size);
            float GetOrthographicSize() const;

            void SetAspectRatio(float aspectRatio);

            // Transform sync control
            void SetSyncWithTransform(bool sync) { syncWithTransform = sync; }
            bool GetSyncWithTransform() const { return syncWithTransform; }

            // Main camera flag
            void SetAsMain(bool isMain) { isMainCamera = isMain; }
            bool IsMain() const { return isMainCamera; }

            const char* GetTypeName() const override { return "CameraComponent"; }

        private:
            void SyncWithTransform();

            std::unique_ptr<Rendering::Camera> camera;
            bool syncWithTransform = true;
            bool isMainCamera = false;
        };

    }
}
