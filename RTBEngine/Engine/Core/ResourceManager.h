#pragma once
#include "../RTBEngineAPI.h"
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
#include <filesystem>

namespace RTBEngine {
    namespace ECS {
        class Scene;
    }
}

namespace RTBEngine {
    namespace Core {

        #pragma warning(push)
        #pragma warning(disable: 4251)
        class RTB_API ResourceManager {
        public:
            // Default asset paths
            static constexpr const char* DEFAULT_TEXTURE_PATH = "Default/Textures/default.png";
            static constexpr const char* DEFAULT_LOGO_PATH = "Default/Textures/logo.png";
            static constexpr const char* DEFAULT_FONT_PATH = "Default/Fonts/SourceSans3-Black.ttf";
            static constexpr const char* DEFAULT_CUBE_PATH = "Default/Models/cube.obj";
            static constexpr const char* DEFAULT_SPHERE_PATH = "Default/Models/sphere.obj";
            static constexpr const char* DEFAULT_PLANE_PATH = "Default/Models/plane.obj";
            static ResourceManager& GetInstance();

            ResourceManager(const ResourceManager&) = delete;
            ResourceManager& operator=(const ResourceManager&) = delete;

            // Shader management
            Rendering::Shader* GetShader(const std::string& name);
            Rendering::Shader* LoadShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath);

            // Texture management
            Rendering::Texture* GetTexture(const std::string& path);
            Rendering::Texture* LoadTexture(const std::string& path, bool flipVertically = true);
            // Textures referenced by imported models (FBX/OBJ) must NOT be flipped again:
            // ModelLoader already converts UVs to OpenGL space via aiProcess_FlipUVs.
            Rendering::Texture* LoadModelTexture(const std::string& path);
            Rendering::Texture* LoadTextureAsset(const std::string& textureFilePath);
            void SetAssetRootPath(const std::filesystem::path& path);
            std::string ResolvePathForRead(const std::string& path) const;
            std::string TryMakeAssetRelativePath(const std::string& path) const;

			// Model management (single mesh - backwards compatible)
            Rendering::Mesh* GetModel(const std::string& path);
            Rendering::Mesh* LoadModel(const std::string& path);

            // Model management (all meshes)
            const std::vector<Rendering::Mesh*>& GetModelMeshes(const std::string& path);
            const std::vector<Rendering::Mesh*>& LoadModelMeshes(const std::string& path);
            void RegisterMeshes(const std::string& path, const std::vector<Rendering::Mesh*>& meshes);

            // Full model data (meshes, skeleton, animations) — cached after first load.
            const Rendering::ModelData& GetModelData(const std::string& path);
            const Rendering::ModelData& LoadModelData(const std::string& path);

            // Audio management
            Audio::AudioClip* GetAudioClip(const std::string& path);
            Audio::AudioClip* LoadAudioClip(const std::string& path, bool stream = false);

			// Font management
			Rendering::Font* GetFont(const std::string& path);
			Rendering::Font* LoadFont(const std::string& path, const float* sizes, int numSizes);
			Rendering::Font* GetDefaultFont();

            // Cubemap management
            Rendering::Cubemap* GetCubemap(const std::string& path);
            Rendering::Cubemap* LoadCubemapAsset(const std::string& cubemapFilePath);

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

            // Register an externally-created texture under a path for serialization.
            // ResourceManager takes ownership.
            void RegisterTexture(const std::string& path, Rendering::Texture* texture);

            //Reverse
            std::string GetTexturePath(Rendering::Texture* texture) const;
            std::string GetAudioClipPath(Audio::AudioClip* clip) const;
            std::string GetMeshPath(Rendering::Mesh* mesh) const;
            std::string GetFontPath(Rendering::Font* font) const;
            std::string GetCubemapPath(Rendering::Cubemap* cubemap) const;

            void Clear();

        private:
            ResourceManager();
            ~ResourceManager();
            static bool IsAssetReferencePath(const std::filesystem::path& path, const std::string& assetDirectoryName);

            std::unordered_map<std::string, std::unique_ptr<Rendering::Shader>> shaders;
            std::unordered_map<std::string, std::unique_ptr<Rendering::Texture>> textures;
            std::unordered_map<std::string, std::vector<std::unique_ptr<Rendering::Mesh>>> modelMeshes;
            std::unordered_map<std::string, std::unique_ptr<Audio::AudioClip>> audioClips;
            std::unordered_map<std::string, std::unique_ptr<Rendering::Cubemap>> cubemaps;

            // Cache for raw pointers (for GetModelMeshes return)
            std::unordered_map<std::string, std::vector<Rendering::Mesh*>> modelMeshPtrs;
            std::unordered_map<std::string, Rendering::ModelData> modelDataCache;
            static std::vector<Rendering::Mesh*> emptyMeshVector;
            static Rendering::ModelData emptyModelData;
			std::unordered_map<std::string, std::unique_ptr<Rendering::Font>> fonts;
            std::unordered_map<std::string, std::unique_ptr<ECS::Scene>> scenes;
			Rendering::Font* defaultFont = nullptr;

            // Reverse lookup maps: pointer -> path (for serialization)
            std::unordered_map<Rendering::Texture*, std::string> texturePathMap;
            std::unordered_map<Audio::AudioClip*, std::string> audioClipPathMap;
            std::unordered_map<Rendering::Mesh*, std::string> meshPathMap;
            std::unordered_map<Rendering::Font*, std::string> fontPathMap;
            std::unordered_map<Rendering::Cubemap*, std::string> cubemapPathMap;


            std::unique_ptr<Rendering::Skybox> defaultSkybox;
            std::filesystem::path assetRootPath;
        };
        #pragma warning(pop)

    }
}
