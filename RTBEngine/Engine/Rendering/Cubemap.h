#pragma once
#include <GL/glew.h>
#include <string>
#include <array>

namespace RTBEngine {
    namespace Rendering {

        class Cubemap {
        public:
            Cubemap();
            ~Cubemap();

            Cubemap(const Cubemap&) = delete;
            Cubemap& operator=(const Cubemap&) = delete;

            // Load 6 faces from folder containing: right.png, left.png, top.png, bottom.png, front.png, back.png
            bool LoadFromFolder(const std::string& folderPath, const std::string& extension = ".png");

            // Load 6 faces from individual file paths
            // Order: right (+X), left (-X), top (+Y), bottom (-Y), front (+Z), back (-Z)
            bool LoadFromFiles(const std::array<std::string, 6>& facePaths);

            // Create a solid color cubemap (for default/fallback skybox)
            bool CreateSolidColor(float r, float g, float b);

            void Bind(unsigned int slot = 0) const;
            void Unbind() const;

            GLuint GetID() const { return textureID; }
            bool IsLoaded() const { return textureID != 0; }

        private:
            GLuint textureID = 0;
        };

    }
}
