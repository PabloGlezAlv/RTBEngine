#include "ResourceManager.h"
#include "../Scripting/SceneLoader.h"
#include "../ECS/Scene.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "../RTBEngine.h"

namespace RTBEngine {
    namespace Core {

        std::vector<Rendering::Mesh*> ResourceManager::emptyMeshVector;

        ResourceManager& ResourceManager::GetInstance()
        {
            static ResourceManager instance;
            return instance;
        }

        ResourceManager::ResourceManager() = default;

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


        void ResourceManager::RegisterTexture(const std::string& path, Rendering::Texture* texture)
        {
            if (!texture) return;
            // Skip if already registered under this path
            auto it = textures.find(path);
            if (it != textures.end() && it->second.get() == texture) return;

            textures[path] = std::unique_ptr<Rendering::Texture>(texture);
            texturePathMap[texture] = path;
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
                RTB_INFO(std::string("[LOAD_MODEL_MESHES] Cache hit for '") + path + "' returning " + std::to_string(it->second.size()) + " meshes");
                return it->second;
            }

            std::vector<Rendering::Mesh*> loadedMeshes = Rendering::ModelLoader::LoadModel(path);
            RTB_INFO(std::string("[LOAD_MODEL_MESHES] Cache miss for '") + path + "'. ModelLoader::LoadModel returned " + std::to_string(loadedMeshes.size()) + " meshes");

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

        void ResourceManager::RegisterMeshes(const std::string& path, const std::vector<Rendering::Mesh*>& meshes)
        {
            RTB_INFO(std::string("[REGISTER_MESHES] path='") + path + "' count=" + std::to_string(meshes.size()));
            if (meshes.empty()) return;

            // Always register each Mesh* in the reverse-lookup map
            for (Rendering::Mesh* mesh : meshes) {
                if (mesh) {
                    meshPathMap[mesh] = path;
                }
            }

            // Always update the raw-pointer list so GetModelMeshes stays consistent with
            // meshPathMap. ConfigureMeshRenderer and ConfigureAnimator allocate Mesh objects
            // via LoadModelWithAnimations (bypassing LoadModelMeshes), so without updating
            // modelMeshPtrs here the list would point to a stale/different set, causing
            // SyncProperties to restore wrong meshes after paste or prefab-drop.
            std::vector<Rendering::Mesh*> meshPtrs;
            meshPtrs.reserve(meshes.size());
            for (Rendering::Mesh* mesh : meshes) {
                if (mesh) meshPtrs.push_back(mesh);
            }
            modelMeshPtrs[path] = std::move(meshPtrs);

            // Take ownership of any pointers not already held by modelMeshes.
            // This covers the case where ConfigureMeshRenderer / ConfigureAnimator created
            // fresh allocations that no other owner would free.
            auto& owned = modelMeshes[path];
            for (Rendering::Mesh* mesh : meshes) {
                if (!mesh) continue;
                bool alreadyOwned = false;
                for (const auto& u : owned) {
                    if (u.get() == mesh) { alreadyOwned = true; break; }
                }
                if (!alreadyOwned) {
                    owned.push_back(std::unique_ptr<Rendering::Mesh>(mesh));
                }
            }
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

        Rendering::Cubemap* ResourceManager::LoadCubemapAsset(const std::string& cubemapFilePath) {
            // Return cached instance if already loaded
            auto existing = GetCubemap(cubemapFilePath);
            if (existing) {
                return existing;
            }

            // Parse .cubemap file — key=value pairs, one per line
            // Expected keys: right, left, top, bottom, front, back
            std::ifstream file(cubemapFilePath);
            if (!file.is_open()) {
                RTB_ERROR("ResourceManager: Failed to open .cubemap file: " + cubemapFilePath);
                return nullptr;
            }

            std::array<std::string, 6> faces;
            static const char* faceKeys[] = { "right", "left", "top", "bottom", "front", "back" };

            std::string line;
            while (std::getline(file, line)) {
                if (line.empty() || line[0] == '#') continue;
                auto sep = line.find('=');
                if (sep == std::string::npos) continue;

                std::string key   = line.substr(0, sep);
                std::string value = line.substr(sep + 1);

                // Trim whitespace
                auto trim = [](std::string& s) {
                    size_t start = s.find_first_not_of(" \t\r\n");
                    size_t end   = s.find_last_not_of(" \t\r\n");
                    s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
                };
                trim(key);
                trim(value);

                for (int i = 0; i < 6; ++i) {
                    if (key == faceKeys[i]) {
                        faces[i] = value;
                        break;
                    }
                }
            }

            // Validate that all 6 faces are assigned
            for (int i = 0; i < 6; ++i) {
                if (faces[i].empty()) {
                    RTB_ERROR("ResourceManager: .cubemap file missing face '" + std::string(faceKeys[i]) + "': " + cubemapFilePath);
                    return nullptr;
                }
            }

            auto cubemap = std::make_unique<Rendering::Cubemap>();
            if (!cubemap->LoadFromFiles(faces)) {
                RTB_ERROR("ResourceManager: Failed to load cubemap faces from: " + cubemapFilePath);
                return nullptr;
            }

            Rendering::Cubemap* ptr = cubemap.get();
            cubemaps[cubemapFilePath] = std::move(cubemap);
            cubemapPathMap[ptr] = cubemapFilePath;
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
            static const std::string key = "__default_cubemap__";
            auto existing = GetCubemap(key);
            if (existing) return existing;

            auto cubemap = std::make_unique<Rendering::Cubemap>();
            cubemap->CreateSolidColor(0.3f, 0.4f, 0.6f);

            Rendering::Cubemap* ptr = cubemap.get();
            cubemaps[key] = std::move(cubemap);
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
            RTB_WARN(std::string("[GET_MESH_PATH WARNING] Mesh ptr not found in pathMap (ptr=") + std::to_string(reinterpret_cast<size_t>(mesh)) + "). meshPathMap has " + std::to_string(meshPathMap.size()) + " entries.");
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