#include "Skybox.h"
#include "Cubemap.h"
#include "Shader.h"
#include "Camera.h"
#include "../Math/Matrix/Matrix4.h"

namespace RTBEngine {
    namespace Rendering {

        // Cube vertices 
        static const float skyboxVertices[] = {
            // Back face (-Z)
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            // Front face (+Z)
            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            // Left face (-X)
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,

            // Right face (+X)
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,

             // Top face (+Y)
             -1.0f,  1.0f,  1.0f,
              1.0f,  1.0f,  1.0f,
              1.0f,  1.0f, -1.0f,
              1.0f,  1.0f, -1.0f,
             -1.0f,  1.0f, -1.0f,
             -1.0f,  1.0f,  1.0f,

             // Bottom face (-Y)
             -1.0f, -1.0f, -1.0f,
              1.0f, -1.0f, -1.0f,
              1.0f, -1.0f,  1.0f,
              1.0f, -1.0f,  1.0f,
             -1.0f, -1.0f,  1.0f,
             -1.0f, -1.0f, -1.0f
        };

        Skybox::Skybox()
            : cubemap(nullptr), shader(nullptr), enabled(true), VAO(0), VBO(0) {
        }

        Skybox::~Skybox() {
            DeleteCubeMesh();
        }

        bool Skybox::Initialize(Cubemap* cubemap, Shader* shader) {
            if (!cubemap || !shader) {
                return false;
            }

            this->cubemap = cubemap;
            this->shader = shader;

            CreateCubeMesh();
            return true;
        }

        void Skybox::SetCubemap(Cubemap* cubemap) {
            this->cubemap = cubemap;
        }

        void Skybox::CreateCubeMesh() {
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);

            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

            // Position attribute (location = 0)
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

            glBindVertexArray(0);
        }

        void Skybox::DeleteCubeMesh() {
            if (VAO != 0) {
                glDeleteVertexArrays(1, &VAO);
                VAO = 0;
            }
            if (VBO != 0) {
                glDeleteBuffers(1, &VBO);
                VBO = 0;
            }
        }

        void Skybox::Render(Camera* camera) {
            if (!enabled || !cubemap || !shader || !camera) {
                return;
            }

            // Change depth function so skybox passes depth test at maximum depth (1.0)
            glDepthFunc(GL_LEQUAL);

            shader->Bind();

            // Remove translation from view matrix (only keep rotation)
            // This makes the skybox appear infinitely far away
            Math::Matrix4 view = camera->GetViewMatrix();
            view.m[12] = 0.0f;  // Remove X translation
            view.m[13] = 0.0f;  // Remove Y translation
            view.m[14] = 0.0f;  // Remove Z translation

            shader->SetMatrix4("uView", view);
            shader->SetMatrix4("uProjection", camera->GetProjectionMatrix());
            shader->SetInt("uSkybox", 0);

            // Bind cubemap and draw
            cubemap->Bind(0);

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);

            cubemap->Unbind();
            shader->Unbind();

            // Restore default depth function
            glDepthFunc(GL_LESS);
        }

    }
}
