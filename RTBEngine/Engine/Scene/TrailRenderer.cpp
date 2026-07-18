#include "TrailRenderer.h"

#include "GameObject.h"
#include "../Core/ResourceManager.h"
#include "../Rendering/Camera.h"
#include "../Rendering/CameraUBO.h"
#include "../Rendering/Shader.h"

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

            if (vao != 0 && vbo != 0) {
                return true;
            }

            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);

            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(TrailVertex), nullptr, GL_DYNAMIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, color));

            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            return true;
        }

        void TrailRenderer::ReleaseRenderResources()
        {
            if (vao != 0) {
                glDeleteVertexArrays(1, &vao);
                vao = 0;
            }
            if (vbo != 0) {
                glDeleteBuffers(1, &vbo);
                vbo = 0;
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

            // Upload the generated triangle vertices for this frame.
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertices.size() * sizeof(TrailVertex)),
                vertices.data(),
                GL_DYNAMIC_DRAW);

            // Preserve the GL state touched by this component so later renderers are unaffected.
            const GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
            const GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
            const GLboolean wasCullFaceEnabled = glIsEnabled(GL_CULL_FACE);
            GLboolean wasDepthMaskEnabled = GL_TRUE;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &wasDepthMaskEnabled);

            glEnable(GL_BLEND); // Enable alpha colors
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_DEPTH_TEST); // Visible just when in front
            glDepthMask(GL_FALSE); // Avoid z-fighting with floor geometry
            glDisable(GL_CULL_FACE); // Visible from both sides

            // The shader only needs the camera view-projection because vertices are already in world space.
            shader->Bind();
            Rendering::CameraUBO::GetInstance().Bind();

            // Draw the uploaded quads as triangles.
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
            glBindVertexArray(0);

            shader->Unbind();

            // Restore the previous GL state after drawing the trail.
            if (wasCullFaceEnabled) {
                glEnable(GL_CULL_FACE);
            } else {
                glDisable(GL_CULL_FACE);
            }
            if (wasDepthTestEnabled) {
                glEnable(GL_DEPTH_TEST);
            } else {
                glDisable(GL_DEPTH_TEST);
            }
            if (wasBlendEnabled) {
                glEnable(GL_BLEND);
            } else {
                glDisable(GL_BLEND);
            }
            glDepthMask(wasDepthMaskEnabled);
        }

    }
}
