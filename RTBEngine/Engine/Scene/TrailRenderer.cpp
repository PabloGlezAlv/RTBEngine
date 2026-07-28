#include "TrailRenderer.h"

#include "GameObject.h"
#include "../Core/ResourceManager.h"
#include "../Core/Time.h"
#include "../Math/Vectors/Vector2.h"
#include "../Rendering/Camera.h"
#include "../Rendering/CameraUBO.h"
#include "../Rendering/Shader.h"
#include "../Rendering/Texture.h"
#include "../Rendering/RHI/RenderDevice.h"

#include <algorithm>
#include <cmath>

namespace RTBEngine {
    namespace Scene {

        namespace {
            constexpr float kMinTrailWidth = 0.001f;
            constexpr float kSegmentEpsilon = 0.000001f;

            struct TrailVertex {
                Math::Vector3 position;
                Math::Vector4 color;
                Math::Vector2 uv;
                float side = 0.0f;
            };
        }

        using ThisClass = TrailRenderer;
        RTB_REGISTER_COMPONENT(TrailRenderer)
            RTB_PROPERTY_RANGE(width, 0.001f, 10.0f)
            RTB_PROPERTY_RANGE(startWidth, -1.0f, 10.0f)
            RTB_PROPERTY_RANGE(endWidth, -1.0f, 10.0f)
            RTB_PROPERTY_COLOR(color)
            RTB_PROPERTY(fadeAlphaAlongLength)
            RTB_PROPERTY(visible)
            RTB_PROPERTY_ENUM(blendMode, "Alpha", "Additive")
            RTB_PROPERTY_ENUM(alignment, "FlatXZ", "CameraFacing")
            RTB_PROPERTY_RANGE(softEdge, 0.0f, 4.0f)
            RTB_PROPERTY_TEXTURE(texture)
            RTB_PROPERTY_RANGE(uvScrollSpeed, -20.0f, 20.0f)
            RTB_PROPERTY_RANGE(uvTilesPerMeter, 0.0f, 10.0f)
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
            if (startWidth >= 0.0f) {
                startWidth = std::max(startWidth, kMinTrailWidth);
            }
            if (endWidth >= 0.0f) {
                endWidth = std::max(endWidth, kMinTrailWidth);
            }
            softEdge = std::max(0.0f, softEdge);
            uvTilesPerMeter = std::max(0.0f, uvTilesPerMeter);
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

        float TrailRenderer::ResolveHalfWidthAtT(float t) const
        {
            const float clampedT = std::clamp(t, 0.0f, 1.0f);
            const float start = (startWidth >= 0.0f) ? startWidth : width;
            const float end = (endWidth >= 0.0f) ? endWidth : width;
            return (start + (end - start) * clampedT) * 0.5f;
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
            device.EnableVertexAttribFloat(2, 2, static_cast<int>(sizeof(TrailVertex)), offsetof(TrailVertex, uv));
            device.EnableVertexAttribFloat(3, 1, static_cast<int>(sizeof(TrailVertex)), offsetof(TrailVertex, side));

            device.UnbindVertexArray();

            return true;
        }

        void TrailRenderer::ReleaseRenderResources()
        {
            if (!Rendering::RHI::RenderDevice::HasDevice()) {
                vao = Rendering::RHI::kInvalidGpuId;
                vbo = Rendering::RHI::kInvalidGpuId;
                shader = nullptr;
                return;
            }

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

            // Recreate GPU layout if an older session still has the previous vertex format.
            // EnsureRenderResources only builds once; attribute count is fixed in this build.

            std::vector<TrailVertex> vertices;
            vertices.reserve((points.size() - 1) * 6);

            const Math::Vector3 up = Math::Vector3::Up();
            const Math::Vector3 cameraPosition = camera->GetPosition();
            const float pointCountMinusOne = points.size() > 1
                ? static_cast<float>(points.size() - 1)
                : 1.0f;

            std::vector<float> cumulativeDistance(points.size(), 0.0f);
            for (std::size_t i = 1; i < points.size(); ++i) {
                cumulativeDistance[i] = cumulativeDistance[i - 1] + (points[i] - points[i - 1]).Length();
            }
            const float totalLength = std::max(cumulativeDistance.back(), kSegmentEpsilon);
            (void)totalLength;
            const float scrollOffset = Core::Time::GetTime() * uvScrollSpeed;

            auto buildVertexColor = [&](std::size_t pointIndex) -> Math::Vector4 {
                Math::Vector4 vertexColor = color;
                if (fadeAlphaAlongLength) {
                    const float alongTrail = static_cast<float>(pointIndex) / pointCountMinusOne;
                    vertexColor.w *= alongTrail;
                }
                vertexColor.w *= globalAlphaScale;
                return vertexColor;
            };

            auto pushVertex = [&](const Math::Vector3& position,
                                  const Math::Vector4& vertexColor,
                                  float u,
                                  float side) {
                TrailVertex vertex{};
                vertex.position = position;
                vertex.color = vertexColor;
                vertex.uv = Math::Vector2(u + scrollOffset, side * 0.5f + 0.5f);
                vertex.side = side;
                vertices.push_back(vertex);
            };

            for (std::size_t i = 0; i + 1 < points.size(); ++i) {
                const Math::Vector3 start = points[i];
                const Math::Vector3 end = points[i + 1];

                Math::Vector3 direction = end - start;
                if (alignment == TrailAlignment::FlatXZ) {
                    direction.y = 0.0f;
                }
                if (direction.LengthSquared() <= kSegmentEpsilon) {
                    continue;
                }

                direction.Normalize();

                Math::Vector3 sideDirection;
                if (alignment == TrailAlignment::CameraFacing) {
                    const Math::Vector3 midPoint = (start + end) * 0.5f;
                    Math::Vector3 toCamera = cameraPosition - midPoint;
                    if (toCamera.LengthSquared() <= kSegmentEpsilon) {
                        toCamera = camera->GetForward() * -1.0f;
                    }
                    sideDirection = toCamera.Cross(direction);
                    if (sideDirection.LengthSquared() <= kSegmentEpsilon) {
                        sideDirection = up.Cross(direction);
                    }
                } else {
                    sideDirection = up.Cross(direction);
                }

                if (sideDirection.LengthSquared() <= kSegmentEpsilon) {
                    continue;
                }
                sideDirection.Normalize();

                const float startT = static_cast<float>(i) / pointCountMinusOne;
                const float endT = static_cast<float>(i + 1) / pointCountMinusOne;
                const Math::Vector3 startSide = sideDirection * ResolveHalfWidthAtT(startT);
                const Math::Vector3 endSide = sideDirection * ResolveHalfWidthAtT(endT);

                const Math::Vector3 p0 = start - startSide;
                const Math::Vector3 p1 = start + startSide;
                const Math::Vector3 p2 = end + endSide;
                const Math::Vector3 p3 = end - endSide;

                const Math::Vector4 startColor = buildVertexColor(i);
                const Math::Vector4 endColor = buildVertexColor(i + 1);
                const float startU = cumulativeDistance[i] * uvTilesPerMeter;
                const float endU = cumulativeDistance[i + 1] * uvTilesPerMeter;

                // Two triangles per segment. side = -1 / +1 drives soft-edge falloff.
                pushVertex(p0, startColor, startU, -1.0f);
                pushVertex(p1, startColor, startU, 1.0f);
                pushVertex(p2, endColor, endU, 1.0f);
                pushVertex(p0, startColor, startU, -1.0f);
                pushVertex(p2, endColor, endU, 1.0f);
                pushVertex(p3, endColor, endU, -1.0f);
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
            device.SetDepthFunc(Rendering::RHI::DepthFunc::LEqual);
            device.SetDepthWrite(false);
            device.SetBlend(true);
            if (blendMode == TrailBlendMode::Additive) {
                device.SetBlendFuncSeparate(
                    Rendering::RHI::BlendFactor::SrcAlpha,
                    Rendering::RHI::BlendFactor::One,
                    Rendering::RHI::BlendFactor::SrcAlpha,
                    Rendering::RHI::BlendFactor::One);
            } else {
                device.SetBlendFuncSeparate(
                    Rendering::RHI::BlendFactor::SrcAlpha,
                    Rendering::RHI::BlendFactor::OneMinusSrcAlpha,
                    Rendering::RHI::BlendFactor::SrcAlpha,
                    Rendering::RHI::BlendFactor::OneMinusSrcAlpha);
            }
            device.SetCullFace(false);

            shader->Bind();
            Rendering::CameraUBO::GetInstance().Bind();
            shader->SetFloat("uSoftEdge", softEdge);
            shader->SetBool("uHasTexture", texture != nullptr);
            if (texture) {
                texture->Bind(0);
                shader->SetInt("uDiffuse", 0);
            }

            device.BindVertexArray(vao);
            device.DrawArrays(
                Rendering::RHI::PrimitiveTopology::Triangles,
                0,
                static_cast<int>(vertices.size()));
            device.UnbindVertexArray();

            if (texture) {
                texture->Unbind();
            }
            shader->Unbind();

            device.SetDepthWrite(true);
            device.SetBlend(false);
            device.SetCullFace(true);
            device.SetDepthFunc(Rendering::RHI::DepthFunc::Less);
            device.SetDepthTest(true);
        }

    }
}
