#pragma once
#include "../Rendering/Shader.h"
#include "../Rendering/Texture.h"
#include "../Rendering/Mesh.h"
#include "../Rendering/ModelLoader.h"
#include "../Rendering/Font.h"
#include "../Audio/AudioClip.h"
#include "../Rendering/Cubemap.h"
#include "../Rendering/Skybox.h"

#include <unordered_map>
#include <string>
#include <memory>

namespace RTBEngine {
    namespace ECS {
        class Scene;
    }
}

namespace RTBEngine {
    namespace Core {

        class ResourceManager {
        public:
            // Default asset paths
            static constexpr const char* DEFAULT_TEXTURE_PATH = "Default/Textures/white.png";
            static constexpr const char* DEFAULT_LOGO_PATH = "Default/Textures/logo.png";
            static constexpr const char* DEFAULT_FONT_PATH = "Default/Fonts/SourceSans3-Black.ttf";
            static constexpr const char* DEFAULT_CUBE_PATH = "Default/Models/cube.obj";
            static constexpr const char* DEFAULT_SPHERE_PATH = "Default/Models/sphere.obj";
            static constexpr const char* DEFAULT_PLANE_PATH = "Default/Models/plane.obj";
            static constexpr const char* DEFAULT_SKYBOX_PATH = "Default/Textures/Skybox";


            static ResourceManager& GetInstance();

            ResourceManager(const ResourceManager&) = delete;
            ResourceManager& operator=(const ResourceManager&) = delete;

            // Shader management
            Rendering::Shader* GetShader(const std::string& name);
            Rendering::Shader* LoadShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath);

            // Texture management
            Rendering::Texture* GetTexture(const std::string& path);
            Rendering::Texture* LoadTexture(const std::string& path);

			// Model management (single mesh - backwards compatible)
            Rendering::Mesh* GetModel(const std::string& path);
            Rendering::Mesh* LoadModel(const std::string& path);

            // Model management (all meshes)
            const std::vector<Rendering::Mesh*>& GetModelMeshes(const std::string& path);
            const std::vector<Rendering::Mesh*>& LoadModelMeshes(const std::string& path);

            // Audio management
            Audio::AudioClip* GetAudioClip(const std::string& path);
            Audio::AudioClip* LoadAudioClip(const std::string& path, bool stream = false);

			// Font management
			Rendering::Font* GetFont(const std::string& path);
			Rendering::Font* LoadFont(const std::string& path, const float* sizes, int numSizes);
			Rendering::Font* GetDefaultFont();

            // Cubemap management
            Rendering::Cubemap* GetCubemap(const std::string& path);
            Rendering::Cubemap* LoadCubemap(const std::string& folderPath, const std::string& extension = ".png");

            // Default resources
            Rendering::Texture* GetDefaultTexture();
            Rendering::Texture* GetLogoTexture();
            Rendering::Mesh* GetDefaultCube();
            Rendering::Mesh* GetDefaultSphere();
            Rendering::Mesh* GetDefaultPlane();
            Rendering::Cubemap* GetDefaultCubemap();
            Rendering::Skybox* GetDefaultSkybox();


            // Scene management
            ECS::Scene* LoadScene(const std::string& path);
            ECS::Scene* GetScene(const std::string& path);

            void Clear();

        private:
            ResourceManager() = default;
            ~ResourceManager();

            std::unordered_map<std::string, std::unique_ptr<Rendering::Shader>> shaders;
            std::unordered_map<std::string, std::unique_ptr<Rendering::Texture>> textures;
            std::unordered_map<std::string, std::vector<std::unique_ptr<Rendering::Mesh>>> modelMeshes;
            std::unordered_map<std::string, std::unique_ptr<Audio::AudioClip>> audioClips;
            std::unordered_map<std::string, std::unique_ptr<Rendering::Cubemap>> cubemaps;

            // Cache for raw pointers (for GetModelMeshes return)
            std::unordered_map<std::string, std::vector<Rendering::Mesh*>> modelMeshPtrs;
            static std::vector<Rendering::Mesh*> emptyMeshVector;
			std::unordered_map<std::string, std::unique_ptr<Rendering::Font>> fonts;
            std::unordered_map<std::string, std::unique_ptr<ECS::Scene>> scenes;
			Rendering::Font* defaultFont = nullptr;

            std::unique_ptr<Rendering::Skybox> defaultSkybox;
        };

    }
}