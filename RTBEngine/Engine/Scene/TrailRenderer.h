#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Math/Vectors/Vector3.h"
#include "../Math/Vectors/Vector4.h"
#include "../Reflection/PropertyMacros.h"
#include "../Rendering/RHI/RenderTypes.h"

#include <cstddef>
#include <vector>

namespace RTBEngine {
    namespace Rendering {
        class Camera;
        class Shader;
        class Texture;
    }
}

namespace RTBEngine {
    namespace Scene {

        enum class TrailBlendMode {
            Alpha = 0,
            Additive = 1
        };

        enum class TrailAlignment {
            FlatXZ = 0,
            CameraFacing = 1
        };

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

            void SetGlobalAlphaScale(float scale) { globalAlphaScale = scale < 0.0f ? 0.0f : scale; }
            float GetGlobalAlphaScale() const { return globalAlphaScale; }

            void Render(Rendering::Camera* camera);

            float width = 0.15f;
            // When >= 0, overrides width at the start / end of the polyline (taper).
            float startWidth = -1.0f;
            float endWidth = -1.0f;
            Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            bool fadeAlphaAlongLength = false;
            bool visible = false;
            TrailBlendMode blendMode = TrailBlendMode::Alpha;
            TrailAlignment alignment = TrailAlignment::FlatXZ;
            // 0 = hard edges, 1 = full soft falloff across the strip width.
            float softEdge = 0.0f;
            Rendering::Texture* texture = nullptr;
            float uvScrollSpeed = 0.0f;
            // Scales U along the polyline (world units → UV).
            float uvTilesPerMeter = 0.25f;

            RTB_COMPONENT(TrailRenderer)

        private:
            std::vector<Math::Vector3> points;
            float globalAlphaScale = 1.0f;
            Rendering::RHI::GpuId vao = Rendering::RHI::kInvalidGpuId;
            Rendering::RHI::GpuId vbo = Rendering::RHI::kInvalidGpuId;
            Rendering::Shader* shader = nullptr;

            bool EnsureRenderResources();
            void ReleaseRenderResources();
            float ResolveHalfWidthAtT(float t) const;
        };
#pragma warning(pop)

    }
}
