#pragma once
#include <GL/glew.h>
#include <vector>
#include "Vertex.h"


// Guide from: https://learnopengl.com/Model-Loading/Mesh
namespace RTBEngine {
    namespace Rendering {

        class Mesh {
        public:
            Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
            ~Mesh();

            Mesh(const Mesh&) = delete;
            Mesh& operator=(const Mesh&) = delete;

            void Draw() const;

            unsigned int GetVertexCount() const { return vertexCount; }
            unsigned int GetIndexCount() const { return indexCount; }

        private:
            void SetupMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);

            GLuint VAO;
            GLuint VBO;
            GLuint EBO;

            unsigned int vertexCount;
            unsigned int indexCount;
        };

    }
}