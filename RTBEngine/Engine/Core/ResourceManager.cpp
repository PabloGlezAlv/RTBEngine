#include "ResourceManager.h"
#include "../Scripting/SceneLoader.h"
#include "../ECS/Scene.h"
#include <iostream>

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
                std::cerr << "Failed to load shader: " << name << std::endl;
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
                std::cerr << "Failed to load texture: " << path << std::endl;
                return nullptr;
            }

            // Store and return
            Rendering::Texture* texturePtr = texture.get();
            textures[path] = std::move(texture);
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
                std::cerr << "ResourceManager: Failed to load model: " << path << std::endl;
                return emptyMeshVector;
            }

            // Store all meshes
            std::vector<std::unique_ptr<Rendering::Mesh>> ownedMeshes;
            std::vector<Rendering::Mesh*> meshPtrs;

            for (Rendering::Mesh* mesh : loadedMeshes) {
                meshPtrs.push_back(mesh);
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
                std::cerr << "ResourceManager: Failed to load audio clip: " << path << std::endl;
                return nullptr;
            }

            Audio::AudioClip* clipPtr = clip.get();
            audioClips[path] = std::move(clip);
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
				std::cerr << "ResourceManager: Failed to load font: " << path << std::endl;
				return nullptr;
			}

			Rendering::Font* fontPtr = font.get();
			fonts[path] = std::move(font);
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
        }
    }
}