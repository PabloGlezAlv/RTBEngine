#pragma once
#include <GL/glew.h>

namespace RTBEngine {
    namespace Rendering {
        class Cubemap;
        class Shader;
        class Camera;
    }
}

//Website to create skyboxes: https://tools.wwwtyro.net/space-3d/index.html
namespace RTBEngine {
    namespace Rendering {

        class Skybox {
        public:
            Skybox();
            ~Skybox();

            Skybox(const Skybox&) = delete;
            Skybox& operator=(const Skybox&) = delete;

            // Initialize with cubemap and shader
            bool Initialize(Cubemap* cubemap, Shader* shader);

            // Change cubemap at runtime
            void SetCubemap(Cubemap* cubemap);
            Cubemap* GetCubemap() const { return cubemap; }

            // Render the skybox
            void Render(Camera* camera);

            // Enable/disable rendering
            void SetEnabled(bool enabled) { this->enabled = enabled; }
            bool IsEnabled() const { return enabled; }

        private:
            void CreateCubeMesh();
            void DeleteCubeMesh();

            Cubemap* cubemap = nullptr;
            Shader* shader = nullptr;
            bool enabled = true;

            // OpenGL buffers for cube mesh
            GLuint VAO = 0;
            GLuint VBO = 0;
        };

    }
}
