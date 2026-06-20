#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Math/Vectors/Vector3.h"
#include "../Math/Vectors/Vector4.h"
#include "../Reflection/PropertyMacros.h"

#include <GL/glew.h>
#include <cstddef>
#include <vector>

namespace RTBEngine {
    namespace Rendering {
        class Camera;
        class Shader;
    }
}

namespace RTBEngine {
    namespace ECS {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API TrailRenderer : public Component {
        public:
            TrailRenderer();
            ~TrailRenderer() override;

            TrailRenderer(const TrailRenderer&) = delete;
            TrailRenderer& operator=(const TrailRenderer&) = delete;

            void OnValidate() override;
            void OnDestroy() override;

            void SetPoints(const std::vector<Math::Vector3>& newPoints);
            void SetPoints(const Math::Vector3* newPoints, std::size_t count);
            bool SetPoint(std::size_t index, const Math::Vector3& point);
            void AddPoint(const Math::Vector3& point);
            void ClearPoints();
            const std::vector<Math::Vector3>& GetPoints() const { return points; }
            std::size_t GetPointCount() const { return points.size(); }

            void SetVisible(bool isVisible) { visible = isVisible; }
            bool IsVisible() const { return visible; }

            void Render(Rendering::Camera* camera);

            float width = 0.15f;
            Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            bool visible = false;

            RTB_COMPONENT(TrailRenderer)

        private:
            std::vector<Math::Vector3> points;
            GLuint vao = 0;
            GLuint vbo = 0;
            Rendering::Shader* shader = nullptr;

            bool EnsureRenderResources();
            void ReleaseRenderResources();
        };
#pragma warning(pop)

    }
}
