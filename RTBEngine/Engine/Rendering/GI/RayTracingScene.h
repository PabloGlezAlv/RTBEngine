#pragma once

#include "../../RTBEngineAPI.h"
#include "../RHI/RenderTypes.h"
#include "../../Math/Matrix/Matrix4.h"
#include <cstdint>
#include <vector>

namespace RTBEngine {
    namespace Scene {
        class Scene;
    }

    namespace Rendering {
        class Mesh;

        namespace GI {

            struct RayTracingMeshInstance {
                Mesh* mesh = nullptr;
                Math::Matrix4 worldMatrix;
                bool skinned = false;
            };

#pragma warning(push)
#pragma warning(disable: 4251)
            // Builds and maintains BLAS/TLAS for DDGI ray queries (Vulkan only).
            class RTB_API RayTracingScene {
            public:
                RayTracingScene() = default;
                ~RayTracingScene();

                void Rebuild(Scene::Scene* scene);
                bool IsValid() const { return valid; }
                void* GetTlasHandle() const { return tlasHandle; }

            private:
                void Clear();

                struct BlasEntry {
                    RHI::GpuId deviceVertexBuffer = RHI::kInvalidGpuId;
                    RHI::GpuId deviceIndexBuffer = RHI::kInvalidGpuId;
                    std::uint32_t indexCount = 0;
                    void* blasHandle = nullptr;
                };

                std::vector<BlasEntry> blasEntries;
                std::vector<RayTracingMeshInstance> instances;
                void* tlasHandle = nullptr;
                bool valid = false;
            };
#pragma warning(pop)

        }
    }
}
