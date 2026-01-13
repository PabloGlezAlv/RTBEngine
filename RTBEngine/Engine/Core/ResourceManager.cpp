#include "ResourceManager.h"
#include "../Scripting/SceneLoader.h"
#include "../ECS/Scene.h"
#include <iostream>
#include "../RTBEngine.h"

namespace RTBEngine {
    namespace Core {

        std::vector<Rendering::Mesh*> ResourceManager::emptyMeshVector;

        ResourceManager& ResourceManager::GetInstance()
        {
            static ResourceManager instance;
            return instance;
        }

        ResourceManager::~ResourceManager()
        {
            Clear();
        }


        Rendering::Shader* ResourceManager::GetShader(const std::string& name)
        {
            auto it = shaders.find(name);
            if (it != shaders.end()) {
                return it->second.get();
            }
            return nullptr;
        }

        Rendering::Shader* ResourceManager::LoadShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath)
        {
            // Check if already loaded
            auto existing = GetShader(name);
            if (existing) {
                return existing;
            }

            // Create new shader
            auto shader = std::make_unique<Rendering::Shader>();
            if (!shader->LoadFromFiles(vertexPath, fragmentPath)) {
                RTB_ERROR("Failed to load shader: " + name);
                return nullptr;
            }

            // Store and return
            Rendering::Shader* shaderPtr = shader.get();
            shaders[name] = std::move(shader);
            return shaderPtr;
        }

        Rendering::Texture* ResourceManager::GetTexture(const std::string& path)
        {
            auto it = textures.find(path);
            if (it != textures.end()) {
                return it->second.get();
            }
            return nullptr;
        }

        Rendering::Texture* ResourceManager::LoadTexture(const std::string& path)
        {
            auto existing = GetTexture(path);
            if (existing) {
                return existing;
            }

            // Create new texture
            auto texture = std::make_unique<Rendering::Texture>();
            if (!texture->LoadFromFile(path)) {
                RTB_ERROR("Failed to load texture: " + path);
                return nullptr;
            }

            // Store and return
            Rendering::Texture* texturePtr = texture.get();
            textures[path] = std::move(texture);
            texturePathMap[texturePtr] = path; 
            return texturePtr;
        }


        Rendering::Mesh* ResourceManager::GetModel(const std::string& path)
        {
            auto it = modelMeshPtrs.find(path);
            if (it != modelMeshPtrs.end() && !it->second.empty()) {
                return it->second[0];
            }
            return nullptr;
        }

        Rendering::Mesh* ResourceManager::LoadModel(const std::string& path)
        {
            const auto& meshes = LoadModelMeshes(path);
            return meshes.empty() ? nullptr : meshes[0];
        }

        const std::vector<Rendering::Mesh*>& ResourceManager::GetModelMeshes(const std::string& path)
        {
            auto it = modelMeshPtrs.find(path);
            if (it != modelMeshPtrs.end()) {
                return it->second;
            }
            return emptyMeshVector;
        }

        const std::vector<Rendering::Mesh*>& ResourceManager::LoadModelMeshes(const std::string& path)
        {
            auto it = modelMeshPtrs.find(path);
            if (it != modelMeshPtrs.end()) {
                return it->second;
            }

            std::vector<Rendering::Mesh*> loadedMeshes = Rendering::ModelLoader::LoadModel(path);

            if (loadedMeshes.empty()) {
                RTB_ERROR("ResourceManager: Failed to load model: " + path);
                return emptyMeshVector;
            }

            // Store all meshes
            std::vector<std::unique_ptr<Rendering::Mesh>> ownedMeshes;
            std::vector<Rendering::Mesh*> meshPtrs;

            for (Rendering::Mesh* mesh : loadedMeshes) {
                meshPtrs.push_back(mesh);
                meshPathMap[mesh] = path;
                ownedMeshes.push_back(std::unique_ptr<Rendering::Mesh>(mesh));
            }

            modelMeshes[path] = std::move(ownedMeshes);
            modelMeshPtrs[path] = meshPtrs;

            return modelMeshPtrs[path];
        }

        Audio::AudioClip* ResourceManager::GetAudioClip(const std::string& path)
        {
            auto it = audioClips.find(path);
            if (it != audioClips.end()) {
                return it->second.get();
            }
            return nullptr;
        }

        Audio::AudioClip* ResourceManager::LoadAudioClip(const std::string& path, bool stream)
        {
            auto it = audioClips.find(path);
            if (it != audioClips.end()) {
                return it->second.get();
            }

            auto clip = std::make_unique<Audio::AudioClip>();
            if (!clip->LoadFromFile(path, stream)) {
                RTB_ERROR("ResourceManager: Failed to load audio clip: " + path);
                return nullptr;
            }

            Audio::AudioClip* clipPtr = clip.get();
            audioClips[path] = std::move(clip);
            audioClipPathMap[clipPtr] = path;
            return clipPtr;
        }

		Rendering::Font* ResourceManager::GetFont(const std::string& path)
		{
			auto it = fonts.find(path);
			if (it != fonts.end()) {
				return it->second.get();
			}
			return nullptr;
		}

		Rendering::Font* ResourceManager::LoadFont(const std::string& path, const float* sizes, int numSizes)
		{
			auto existing = GetFont(path);
			if (existing) {
				return existing;
			}

			auto font = std::make_unique<Rendering::Font>();
			if (!font->LoadFromFile(path, sizes, numSizes)) {
				RTB_ERROR("ResourceManager: Failed to load font: " + path);
				return nullptr;
			}

			Rendering::Font* fontPtr = font.get();
			fonts[path] = std::move(font);
            fontPathMap[fontPtr] = path;
			return fontPtr;
		}

		Rendering::Font* ResourceManager::GetDefaultFont()
		{
			if (!defaultFont) {
				const float defaultSizes[] = { 16.0f, 18.0f, 20.0f, 24.0f, 28.0f, 32.0f };
				defaultFont = LoadFont(DEFAULT_FONT_PATH, defaultSizes, 6);
			}
			return defaultFont;
		}

        Rendering::Cubemap* ResourceManager::GetCubemap(const std::string& path) {
            auto it = cubemaps.find(path);
            if (it != cubemaps.end()) {
                return it->second.get();
            }
            return nullptr;
        }

        Rendering::Cubemap* ResourceManager::LoadCubemap(const std::string& folderPath, const std::string& extension) {
            // Check cache first
            auto existing = GetCubemap(folderPath);
            if (existing) {
                return existing;
            }

            // Load new cubemap
            auto cubemap = std::make_unique<Rendering::Cubemap>();
            if (!cubemap->LoadFromFolder(folderPath, extension)) {
                RTB_ERROR("Failed to load cubemap from: " + folderPath);
                return nullptr;
            }

            Rendering::Cubemap* ptr = cubemap.get();
            cubemaps[folderPath] = std::move(cubemap);
            cubemapPathMap[ptr] = folderPath;
            return ptr;
        }

        Rendering::Texture* ResourceManager::GetDefaultTexture()
        {
            return LoadTexture(DEFAULT_TEXTURE_PATH);
        }

        Rendering::Texture* ResourceManager::GetLogoTexture()
        {
            return LoadTexture(DEFAULT_LOGO_PATH);
        }

        Rendering::Mesh* ResourceManager::GetDefaultCube()
        {
            return LoadModel(DEFAULT_CUBE_PATH);
        }

        Rendering::Mesh* ResourceManager::GetDefaultSphere()
        {
            return LoadModel(DEFAULT_SPHERE_PATH);
        }

        Rendering::Mesh* ResourceManager::GetDefaultPlane()
        {
            return LoadModel(DEFAULT_PLANE_PATH);
        }

        Rendering::Cubemap* ResourceManager::GetDefaultCubemap() {
            // Check if already loaded
            auto existing = GetCubemap(DEFAULT_SKYBOX_PATH);
            if (existing) {
                return existing;
            }

            // Try to load from folder
            auto cubemap = std::make_unique<Rendering::Cubemap>();
            if (!cubemap->LoadFromFolder(DEFAULT_SKYBOX_PATH)) {
                // Fallback: create solid color cubemap (blue-gray sky)
                cubemap->CreateSolidColor(0.3f, 0.4f, 0.6f);
            }

            Rendering::Cubemap* ptr = cubemap.get();
            cubemaps[DEFAULT_SKYBOX_PATH] = std::move(cubemap);
            return ptr;
        }

        Rendering::Skybox* ResourceManager::GetDefaultSkybox() {
            // Lazy initialization
            if (!defaultSkybox) {
                Rendering::Shader* skyboxShader = GetShader("skybox");
                Rendering::Cubemap* cubemap = GetDefaultCubemap();

                if (skyboxShader && cubemap) {
                    defaultSkybox = std::make_unique<Rendering::Skybox>();
                    defaultSkybox->Initialize(cubemap, skyboxShader);
                }
            }
            return defaultSkybox.get();
        }

        ECS::Scene* ResourceManager::LoadScene(const std::string& path) {
            auto it = scenes.find(path);
            if (it != scenes.end()) {
                return it->second.get();
            }

            ECS::Scene* scene = Scripting::SceneLoader::LoadScene(path);
            if (scene) {
                scenes[path] = std::unique_ptr<ECS::Scene>(scene);
            }
            return scene;
        }

        ECS::Scene* ResourceManager::GetScene(const std::string& path)
        {
            auto it = scenes.find(path);
            if (it != scenes.end()) {
                return it->second.get();
            }
            return nullptr;
        }

        std::string ResourceManager::GetTexturePath(Rendering::Texture* texture) const {
            if (!texture) return "";

            auto it = texturePathMap.find(texture);
            if (it != texturePathMap.end()) {
                return it->second;
            }
            return "";
        }

        std::string ResourceManager::GetAudioClipPath(Audio::AudioClip* clip) const {
            if (!clip) return "";

            auto it = audioClipPathMap.find(clip);
            if (it != audioClipPathMap.end()) {
                return it->second;
            }
            return "";
        }

        std::string ResourceManager::GetMeshPath(Rendering::Mesh* mesh) const {
            if (!mesh) return "";

            auto it = meshPathMap.find(mesh);
            if (it != meshPathMap.end()) {
                return it->second;
            }
            return "";
        }

        std::string ResourceManager::GetFontPath(Rendering::Font* font) const {
            if (!font) return "";

            auto it = fontPathMap.find(font);
            if (it != fontPathMap.end()) {
                return it->second;
            }
            return "";
        }

        std::string ResourceManager::GetCubemapPath(Rendering::Cubemap* cubemap) const {
            if (!cubemap) return "";

            auto it = cubemapPathMap.find(cubemap);
            if (it != cubemapPathMap.end()) {
                return it->second;
            }
            return "";
        }


        void ResourceManager::Clear()
        {
            shaders.clear();
            textures.clear();
            modelMeshPtrs.clear();
            modelMeshes.clear();
            audioClips.clear();
            fonts.clear();
            scenes.clear();
            defaultFont = nullptr;
            cubemaps.clear();
            defaultSkybox.reset();

            texturePathMap.clear();
            audioClipPathMap.clear();
            meshPathMap.clear();
            fontPathMap.clear();
            cubemapPathMap.clear();
        }

    }
}