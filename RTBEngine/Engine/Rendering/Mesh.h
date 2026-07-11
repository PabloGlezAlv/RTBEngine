#pragma once
#include "../RTBEngineAPI.h"
#include <GL/glew.h>
#include <cstddef>
#include <vector>
#include "Vertex.h"
#include "../Math/Vectors/Vector3.h"
#include "../Math/Matrix/Matrix4.h"


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
            void DrawInstanced(GLsizei instanceCount) const;

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

            GLuint VAO;
            GLuint VBO;
            GLuint EBO;
            GLuint instanceVBO = 0;

            unsigned int vertexCount;
            unsigned int indexCount;

            // Bounding box
            Math::Vector3 aabbMin;
            Math::Vector3 aabbMax;

            // Material index (-1 = no material)
            int materialIndex = -1;
        };

    }
}