#pragma once
#include "../RTBEngineAPI.h"
#include <cstddef>
#include <vector>
#include "Vertex.h"
#include "../Math/Vectors/Vector3.h"
#include "../Math/Vectors/Vector4.h"
#include "../Math/Matrix/Matrix4.h"
#include "RHI/RenderTypes.h"


// Guide from: https://learnopengl.com/Model-Loading/Mesh
namespace RTBEngine {
    namespace Rendering {

        class RTB_API Mesh {
        public:
            Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
            ~Mesh();

            Mesh(const Mesh&) = delete;
            Mesh& operator=(const Mesh&) = delete;

            void Draw() const;

            // Instanced rendering: uploads per-instance model matrices to this mesh's instance
            // buffer (configuring attributes 5-8 with divisor 1 on first use) and issues a single
            // instanced draw. Used to batch identical non-skinned opaque meshes.
            void UploadInstanceData(const Math::Matrix4* matrices, std::size_t count);
            // Optional per-instance colors (attribute location 9). Call after UploadInstanceData
            // with the same instance count when the shader enables uUseInstanceColor.
            void UploadInstanceColors(const Math::Vector4* colors, std::size_t count);
            void DrawInstanced(int instanceCount) const;

            unsigned int GetVertexCount() const { return vertexCount; }
            unsigned int GetIndexCount() const { return indexCount; }

            // AABB (Axis-Aligned Bounding Box)
            Math::Vector3 GetAABBMin() const { return aabbMin; }
            Math::Vector3 GetAABBMax() const { return aabbMax; }
            Math::Vector3 GetAABBSize() const { return aabbMax - aabbMin; }
            Math::Vector3 GetAABBCenter() const { return (aabbMin + aabbMax) * 0.5f; }

            // Material index (from model file)
            void SetMaterialIndex(int index) { materialIndex = index; }
            int GetMaterialIndex() const { return materialIndex; }

        private:
            void SetupMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
            void CalculateAABB(const std::vector<Vertex>& vertices);

            RHI::GpuId VAO = RHI::kInvalidGpuId;
            RHI::GpuId VBO = RHI::kInvalidGpuId;
            RHI::GpuId EBO = RHI::kInvalidGpuId;
            RHI::GpuId instanceVBO = RHI::kInvalidGpuId;
            RHI::GpuId instanceColorVBO = RHI::kInvalidGpuId;

            unsigned int vertexCount = 0;
            unsigned int indexCount = 0;

            Math::Vector3 aabbMin;
            Math::Vector3 aabbMax;

            int materialIndex = -1;
        };

    }
}
