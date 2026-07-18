#include "TrailRenderer.h"

#include "GameObject.h"
#include "../Core/ResourceManager.h"
#include "../Rendering/Camera.h"
#include "../Rendering/CameraUBO.h"
#include "../Rendering/Shader.h"
#include "../Rendering/RHI/RenderDevice.h"

#include <algorithm>

namespace RTBEngine {
    namespace Scene {

        namespace {
            constexpr float kMinTrailWidth = 0.001f;
            constexpr float kSegmentEpsilon = 0.000001f;

            struct TrailVertex {
                Math::Vector3 position;
                Math::Vector4 color;
            };
        }

        using ThisClass = TrailRenderer;
        RTB_REGISTER_COMPONENT(TrailRenderer)
            RTB_PROPERTY_RANGE(width, 0.001f, 10.0f)
            RTB_PROPERTY_COLOR(color)
            RTB_PROPERTY(fadeAlphaAlongLength)
            RTB_PROPERTY(visible)
        RTB_END_REGISTER(TrailRenderer)

        TrailRenderer::TrailRenderer()
            : Component()
        {
        }

        TrailRenderer::~TrailRenderer()
        {
            ReleaseRenderResources();
        }

        void TrailRenderer::OnValidate()
        {
            width = std::max(width, kMinTrailWidth);
        }

        void TrailRenderer::OnDestroy()
        {
            ReleaseRenderResources();
        }

        void TrailRenderer::SetPoints(const std::vector<Math::Vector3>& newPoints)
        {
            points = newPoints;
        }

        void TrailRenderer::SetPoints(const Math::Vector3* newPoints, std::size_t count)
        {
            points.clear();
            if (!newPoints || count == 0) {
                return;
            }

            points.assign(newPoints, newPoints + count);
        }

        bool TrailRenderer::SetPoint(std::size_t index, const Math::Vector3& point)
        {
            if (index >= points.size()) {
                return false;
            }

            points[index] = point;
            return true;
        }

        void TrailRenderer::AddPoint(const Math::Vector3& point)
        {
            points.push_back(point);
        }

        void TrailRenderer::ClearPoints()
        {
            points.clear();
        }

        bool TrailRenderer::EnsureRenderResources()
        {
            if (!shader) {
                shader = Core::ResourceManager::GetInstance().GetShader("trail_2d");
                if (!shader) {
                    shader = Core::ResourceManager::GetInstance().LoadShader(
                        "trail_2d",
                        "Default/Shaders/trail_2d.vert",
                        "Default/Shaders/trail_2d.frag");
                }
            }

            if (!shader) {
                return false;
            }

            if (vao != Rendering::RHI::kInvalidGpuId && vbo != Rendering::RHI::kInvalidGpuId) {
                return true;
            }

            auto& device = Rendering::RHI::RenderDevice::Get();
            vao = device.CreateVertexArray();
            vbo = device.CreateBuffer();

            device.BindVertexArray(vao);
            device.SetArrayBufferData(
                vbo,
                nullptr,
                sizeof(TrailVertex),
                Rendering::RHI::BufferUsage::Dynamic);

            device.EnableVertexAttribFloat(0, 3, static_cast<int>(sizeof(TrailVertex)), offsetof(TrailVertex, position));
            device.EnableVertexAttribFloat(1, 4, static_cast<int>(sizeof(TrailVertex)), offsetof(TrailVertex, color));

            device.UnbindVertexArray();

            return true;
        }

        void TrailRenderer::ReleaseRenderResources()
        {
            auto& device = Rendering::RHI::RenderDevice::Get();
            if (vao != Rendering::RHI::kInvalidGpuId) {
                device.DestroyVertexArray(vao);
                vao = Rendering::RHI::kInvalidGpuId;
            }
            if (vbo != Rendering::RHI::kInvalidGpuId) {
                device.DestroyBuffer(vbo);
                vbo = Rendering::RHI::kInvalidGpuId;
            }
            shader = nullptr;
        }

        void TrailRenderer::Render(Rendering::Camera* camera)
        {
            if (!isEnabled || !visible || !camera || points.size() < 2) {
                return;
            }

            if (!GetOwner() || !GetOwner()->IsActiveInHierarchy()) {
                return;
            }

            width = std::max(width, kMinTrailWidth);

            if (!EnsureRenderResources()) {
                return;
            }

            std::vector<TrailVertex> vertices;
            vertices.reserve((points.size() - 1) * 6);

            const float halfWidth = width * 0.5f;
            const Math::Vector3 up = Math::Vector3::Up();
            const float pointCountMinusOne = points.size() > 1
                ? static_cast<float>(points.size() - 1)
                : 1.0f;

            auto buildVertexColor = [&](std::size_t pointIndex) -> Math::Vector4 {
                Math::Vector4 vertexColor = color;
                if (fadeAlphaAlongLength) {
                    const float alongTrail = static_cast<float>(pointIndex) / pointCountMinusOne;
                    vertexColor.w *= alongTrail;
                }
                vertexColor.w *= globalAlphaScale;
                return vertexColor;
            };

            for (std::size_t i = 0; i + 1 < points.size(); ++i) {
                const Math::Vector3 start = points[i];
                const Math::Vector3 end = points[i + 1];

                // V1 draws each 2D segment as a flat quad on the world XZ plane.
                Math::Vector3 direction = end - start;
                direction.y = 0.0f;
                if (direction.LengthSquared() <= kSegmentEpsilon) {
                    continue;
                }

                direction.Normalize();
                Math::Vector3 side = up.Cross(direction);
                if (side.LengthSquared() <= kSegmentEpsilon) {
                    continue;
                }
                side.Normalize();
                side *= halfWidth;

                const Math::Vector3 p0 = start - side;
                const Math::Vector3 p1 = start + side;
                const Math::Vector3 p2 = end + side;
                const Math::Vector3 p3 = end - side;

                const Math::Vector4 startColor = buildVertexColor(i);
                const Math::Vector4 endColor = buildVertexColor(i + 1);

                // Two triangles per segment. This keeps width stable across platforms.
                vertices.push_back({ p0, startColor });
                vertices.push_back({ p1, startColor });
                vertices.push_back({ p2, endColor });
                vertices.push_back({ p0, startColor });
                vertices.push_back({ p2, endColor });
                vertices.push_back({ p3, endColor });
            }

            if (vertices.empty()) {
                return;
            }

            auto& device = Rendering::RHI::RenderDevice::Get();
            device.SetArrayBufferData(
                vbo,
                vertices.data(),
                vertices.size() * sizeof(TrailVertex),
                Rendering::RHI::BufferUsage::Dynamic);

            device.SetDepthTest(true);
            device.SetDepthWrite(false);
            device.SetBlend(true);
            device.SetBlendFuncSeparate(
                Rendering::RHI::BlendFactor::SrcAlpha,
                Rendering::RHI::BlendFactor::OneMinusSrcAlpha,
                Rendering::RHI::BlendFactor::SrcAlpha,
                Rendering::RHI::BlendFactor::OneMinusSrcAlpha);
            device.SetCullFace(false);

            shader->Bind();
            Rendering::CameraUBO::GetInstance().Bind();

            device.BindVertexArray(vao);
            device.DrawArrays(
                Rendering::RHI::PrimitiveTopology::Triangles,
                0,
                static_cast<int>(vertices.size()));
            device.UnbindVertexArray();

            shader->Unbind();

            device.SetDepthWrite(true);
            device.SetBlend(false);
            device.SetCullFace(true);
            device.SetDepthTest(true);
        }

    }
}
