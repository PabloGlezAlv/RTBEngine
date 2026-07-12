#include "ResourceManager.h"
#include "../Scripting/SceneLoader.h"
#include "../Scripting/DataAssetLoader.h"
#include "../Data/DataAsset.h"
#include "../Data/DataAssetRegistry.h"
#include "../Scene/Scene.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <array>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include "../RTBEngine.h"
#include "../Rendering/ShaderAsset.h"

namespace RTBEngine {
    namespace Core {

        namespace {
            constexpr char kNoFlipCacheSuffix[] = "\x1fnoflip";

            std::string TextureCacheKey(const std::string& path, bool flipVertically)
            {
                return flipVertically ? path : (path + kNoFlipCacheSuffix);
            }
        }

        std::vector<Rendering::Mesh*> ResourceManager::emptyMeshVector;
        Rendering::ModelData ResourceManager::emptyModelData;

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

        bool ResourceManager::IsAssetReferencePath(const std::filesystem::path& path, const std::string& assetDirectoryName)
        {
            if (assetDirectoryName.empty()) {
                return false;
            }

            const std::string normalized = path.generic_string();
            return normalized == assetDirectoryName ||
                normalized.rfind(assetDirectoryName + "/", 0) == 0;
        }

        void ResourceManager::SetAssetRootPath(const std::filesystem::path& path)
        {
            if (path.empty()) {
                assetRootPath.clear();
                return;
            }

            std::error_code ec;
            std::filesystem::path absolutePath = path;
            if (!absolutePath.is_absolute()) {
                absolutePath = std::filesystem::absolute(absolutePath, ec);
                if (ec) {
                    absolutePath = path;
                }
            }

            assetRootPath = absolutePath.lexically_normal();
        }

        std::string ResourceManager::ResolvePathForRead(const std::string& path) const
        {
            if (path.empty()) {
                return path;
            }

            std::filesystem::path fsPath(path);
            if (fsPath.is_absolute()) {
                return fsPath.lexically_normal().string();
            }

            if (!assetRootPath.empty()) {
                const std::string assetDirectoryName = assetRootPath.filename().generic_string();
                if (IsAssetReferencePath(fsPath, assetDirectoryName)) {
                    std::filesystem::path resolvedPath = assetRootPath;
                    const std::string normalized = fsPath.generic_string();
                    if (normalized.size() > assetDirectoryName.size()) {
                        resolvedPath /= normalized.substr(assetDirectoryName.size() + 1);
                    }
                    return resolvedPath.lexically_normal().string();
                }
            }

            return fsPath.lexically_normal().string();
        }

        std::string ResourceManager::TryMakeAssetRelativePath(const std::string& path) const
        {
            if (path.empty() || assetRootPath.empty()) {
                return "";
            }

            std::filesystem::path fsPath(path);
            const std::string assetDirectoryName = assetRootPath.filename().generic_string();

            if (IsAssetReferencePath(fsPath, assetDirectoryName)) {
                return fsPath.generic_string();
            }

            if (!fsPath.is_absolute()) {
                return "";
            }

            std::error_code ec;
            const std::filesystem::path normalizedAbsolute = fsPath.lexically_normal();
            const std::filesystem::path relative =
                std::filesystem::relative(normalizedAbsolute, assetRootPath, ec);

            if (ec || relative.empty()) {
                return "";
            }

            const std::string relativeString = relative.generic_string();
            if (relativeString.rfind("..", 0) == 0) {
                return "";
            }

            return (std::filesystem::path(assetDirectoryName) / relative).generic_string();
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
            const std::string resolvedVertexPath = ResolvePathForRead(vertexPath);
            const std::string resolvedFragmentPath = ResolvePathForRead(fragmentPath);
            if (!shader->LoadFromFiles(resolvedVertexPath, resolvedFragmentPath)) {
                RTB_ERROR("Failed to load shader: " + name
                    + " (vertex: " + resolvedVertexPath
                    + ", fragment: " + resolvedFragmentPath + ")");
                return nullptr;
            }

            // Store and return
            Rendering::Shader* shaderPtr = shader.get();
            shaders[name] = std::move(shader);
            return shaderPtr;
        }

        bool ResourceManager::IsShaderAssetRef(const std::string& shaderRef)
        {
            if (shaderRef.size() < 8) {
                return false;
            }

            std::string lower = shaderRef;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return lower.size() >= 7 && lower.substr(lower.size() - 7) == ".shader";
        }

        std::vector<std::string> ResourceManager::GetBuiltinMeshShaderNames()
        {
            return { "basic" };
        }

        Rendering::Shader* ResourceManager::LoadShaderAsset(const std::string& assetRef, bool forceReload)
        {
            if (assetRef.empty()) {
                return nullptr;
            }

            const std::string normalizedRef = TryMakeAssetRelativePath(assetRef);
            const std::string lookupKey = normalizedRef.empty() ? assetRef : normalizedRef;
            const std::string parseRef = lookupKey;

            Rendering::ShaderAssetData assetData;
            if (!Rendering::ShaderAsset::ParseFile(parseRef, assetData)) {
                RTB_ERROR("Failed to parse shader asset: " + parseRef);
                shaderAssetDataCache.erase(lookupKey);
                return nullptr;
            }
            shaderAssetDataCache[lookupKey] = assetData;

            if (!forceReload) {
                if (Rendering::Shader* existing = GetShader(lookupKey)) {
                    return existing;
                }
            } else {
                shaders.erase(lookupKey);
            }

            return LoadShader(lookupKey, assetData.vertexPath, assetData.fragmentPath);
        }

        bool ResourceManager::TryGetShaderAssetData(
            const std::string& shaderRef,
            const Rendering::ShaderAssetData** outData) const
        {
            if (!outData) {
                return false;
            }

            *outData = nullptr;
            if (!IsShaderAssetRef(shaderRef)) {
                return false;
            }

            const std::string normalizedRef = TryMakeAssetRelativePath(shaderRef);
            const std::string lookupKey = normalizedRef.empty() ? shaderRef : normalizedRef;

            const auto it = shaderAssetDataCache.find(lookupKey);
            if (it == shaderAssetDataCache.end()) {
                return false;
            }

            *outData = &it->second;
            return true;
        }

        void ResourceManager::ReloadAllShaderAssets()
        {
            const std::vector<std::string> assetRefs = shaderAssetRefs;
            for (const std::string& assetRef : assetRefs) {
                shaderAssetDataCache.erase(assetRef);
                shaders.erase(assetRef);
                LoadShaderAsset(assetRef, true);
            }
        }

        Rendering::Shader* ResourceManager::ResolveShader(const std::string& shaderRef)
        {
            if (shaderRef.empty()) {
                return GetShader("basic");
            }

            if (IsShaderAssetRef(shaderRef)) {
                const std::string normalizedRef = TryMakeAssetRelativePath(shaderRef);
                const std::string lookupRef = normalizedRef.empty() ? shaderRef : normalizedRef;
                if (Rendering::Shader* assetShader = LoadShaderAsset(lookupRef)) {
                    return assetShader;
                }
                return GetShader("basic");
            }

            if (Rendering::Shader* shader = GetShader(shaderRef)) {
                return shader;
            }

            return GetShader("basic");
        }

        void ResourceManager::ScanShaderAssets(const std::filesystem::path& assetsDirectory)
        {
            shaderAssetRefs.clear();
            if (!std::filesystem::exists(assetsDirectory) ||
                !std::filesystem::is_directory(assetsDirectory)) {
                return;
            }

            for (const auto& entry :
                std::filesystem::recursive_directory_iterator(assetsDirectory)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                if (entry.path().extension() != ".shader") {
                    continue;
                }

                std::error_code ec;
                std::filesystem::path relativePath =
                    std::filesystem::relative(entry.path(), assetsDirectory, ec);
                if (ec) {
                    continue;
                }

                std::string assetRef =
                    (std::filesystem::path("Assets") / relativePath).generic_string();
                shaderAssetRefs.push_back(assetRef);
            }

            std::sort(shaderAssetRefs.begin(), shaderAssetRefs.end());
            shaderAssetRefs.erase(
                std::unique(shaderAssetRefs.begin(), shaderAssetRefs.end()),
                shaderAssetRefs.end());
        }

        std::vector<std::string> ResourceManager::GetMeshShaderOptions() const
        {
            std::vector<std::string> options = GetBuiltinMeshShaderNames();
            options.insert(options.end(), shaderAssetRefs.begin(), shaderAssetRefs.end());
            return options;
        }

        Rendering::Texture* ResourceManager::GetTexture(const std::string& path)
        {
            auto it = textures.find(TextureCacheKey(path, true));
            if (it != textures.end()) {
                return it->second.get();
            }

            it = textures.find(TextureCacheKey(path, false));
            if (it != textures.end()) {
                return it->second.get();
            }

            return nullptr;
        }

        Rendering::Texture* ResourceManager::LoadTexture(const std::string& path, bool flipVertically)
        {
            const std::string cacheKey = TextureCacheKey(path, flipVertically);
            auto it = textures.find(cacheKey);
            if (it != textures.end()) {
                return it->second.get();
            }

            auto texture = std::make_unique<Rendering::Texture>();
            const std::string resolvedPath = ResolvePathForRead(path);
            if (!texture->LoadFromFile(resolvedPath, flipVertically)) {
                RTB_ERROR("Failed to load texture: " + path);
                return nullptr;
            }

            Rendering::Texture* texturePtr = texture.get();
            textures[cacheKey] = std::move(texture);
            texturePathMap[texturePtr] = path;
            return texturePtr;
        }


        Rendering::Texture* ResourceManager::LoadModelTexture(const std::string& path)
        {
            // Model UVs are already flipped to OpenGL space by aiProcess_FlipUVs in ModelLoader,
            // so the source image must be loaded without an additional vertical flip. Flipping
            // here as well would double-flip and misalign atlas/palette textures.
            return LoadTexture(path, false);
        }

        Rendering::Texture* ResourceManager::LoadTextureAsset(const std::string& textureFilePath)
        {
            // Check cache first (keyed by .texture file path)
            auto existing = GetTexture(textureFilePath);
            if (existing) {
                return existing;
            }

            // Parse .texture file (key=value format)
            const std::string resolvedTextureFilePath = ResolvePathForRead(textureFilePath);
            std::ifstream file(resolvedTextureFilePath);
            if (!file.is_open()) {
                RTB_ERROR("ResourceManager: Failed to open .texture file: " + textureFilePath);
                return nullptr;
            }

            std::string imagePath;
            bool flip = true;

            std::string line;
            while (std::getline(file, line)) {
                if (line.empty() || line[0] == '#') continue;
                auto sep = line.find('=');
                if (sep == std::string::npos) continue;

                std::string key = line.substr(0, sep);
                std::string value = line.substr(sep + 1);

                // Trim whitespace
                auto trim = [](std::string& s) {
                    size_t start = s.find_first_not_of(" \t\r\n");
                    size_t end = s.find_last_not_of(" \t\r\n");
                    s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
                };
                trim(key);
                trim(value);

                if (key == "image") {
                    imagePath = value;
                } else if (key == "flip") {
                    flip = (value == "true" || value == "1");
                }
            }

            if (imagePath.empty()) {
                RTB_ERROR("ResourceManager: .texture file missing 'image' field: " + textureFilePath);
                return nullptr;
            }

            // Load the actual image with the specified flip setting
            auto texture = std::make_unique<Rendering::Texture>();
            const std::string resolvedImagePath = ResolvePathForRead(imagePath);
            if (!texture->LoadFromFile(resolvedImagePath, flip)) {
                RTB_ERROR("ResourceManager: Failed to load image from .texture asset: " + imagePath);
                return nullptr;
            }

            // Cache under the .texture file path so serialization round-trips correctly
            Rendering::Texture* texturePtr = texture.get();
            textures[textureFilePath] = std::move(texture);
            texturePathMap[texturePtr] = textureFilePath;
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
                    return it->second;
            }

            const std::string resolvedPath = ResolvePathForRead(path);
            std::vector<Rendering::Mesh*> loadedMeshes = Rendering::ModelLoader::LoadModel(resolvedPath);

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

        const Rendering::ModelData& ResourceManager::GetModelData(const std::string& path)
        {
            auto it = modelDataCache.find(path);
            if (it != modelDataCache.end()) {
                return it->second;
            }
            return emptyModelData;
        }

        const Rendering::ModelData& ResourceManager::LoadModelData(const std::string& path)
        {
            if (path.empty()) {
                return emptyModelData;
            }

            auto it = modelDataCache.find(path);
            if (it != modelDataCache.end()) {
                return it->second;
            }

            Rendering::ModelData loaded =
                Rendering::ModelLoader::LoadModelWithAnimations(ResolvePathForRead(path));
            if (loaded.meshes.empty() && loaded.animations.empty() && !loaded.skeleton) {
                RTB_ERROR("ResourceManager: Failed to load model data: " + path);
                return emptyModelData;
            }

            if (!loaded.meshes.empty()) {
                RegisterMeshes(path, loaded.meshes);
                loaded.meshes = modelMeshPtrs[path];
            }

            loaded.modelAssetPath = path;

            auto [insertedIt, inserted] = modelDataCache.emplace(path, std::move(loaded));
            return insertedIt->second;
        }

        const Rendering::ModelData& ResourceManager::LoadAnimationClips(const std::string& path)
        {
            if (path.empty()) {
                return emptyModelData;
            }

            auto it = modelDataCache.find(path);
            if (it != modelDataCache.end()) {
                return it->second;
            }

            Rendering::ModelData loaded =
                Rendering::ModelLoader::LoadModelWithAnimations(ResolvePathForRead(path));
            if (loaded.animations.empty()) {
                for (Rendering::Mesh* mesh : loaded.meshes) {
                    delete mesh;
                }
                RTB_WARN("ResourceManager: Animation source has no clips: " + path);
                return emptyModelData;
            }

            for (Rendering::Mesh* mesh : loaded.meshes) {
                delete mesh;
            }
            loaded.meshes.clear();
            loaded.modelAssetPath = path;

            auto [insertedIt, inserted] = modelDataCache.emplace(path, std::move(loaded));
            return insertedIt->second;
        }

        void ResourceManager::RegisterMeshes(const std::string& path, const std::vector<Rendering::Mesh*>& meshes)
        {
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
            const std::string resolvedPath = ResolvePathForRead(path);
            if (!clip->LoadFromFile(resolvedPath, stream)) {
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
            const std::string resolvedPath = ResolvePathForRead(path);
			if (!font->LoadFromFile(resolvedPath, sizes, numSizes)) {
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
            const std::string resolvedCubemapFilePath = ResolvePathForRead(cubemapFilePath);
            std::ifstream file(resolvedCubemapFilePath);
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
                        faces[i] = ResolvePathForRead(value);
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

        Data::DataAsset* ResourceManager::GetDataAsset(const std::string& path)
        {
            const auto it = dataAssets.find(path);
            return it != dataAssets.end() ? it->second.get() : nullptr;
        }

        Data::DataAsset* ResourceManager::LoadDataAsset(const std::string& path)
        {
            if (Data::DataAsset* existing = GetDataAsset(path)) {
                return existing;
            }

            std::unique_ptr<Data::DataAsset> loaded = Scripting::DataAssetLoader::Load(path);
            if (!loaded) {
                return nullptr;
            }

            Data::DataAsset* ptr = loaded.get();
            dataAssets[path] = std::move(loaded);
            dataAssetPathMap[ptr] = path;
            return ptr;
        }

        void ResourceManager::EvictDataAsset(const std::string& path)
        {
            const auto it = dataAssets.find(path);
            if (it == dataAssets.end()) {
                return;
            }

            dataAssetPathMap.erase(it->second.get());
            dataAssets.erase(it);
        }

        std::string ResourceManager::GetDataAssetPath(const Data::DataAsset* asset) const
        {
            if (!asset) {
                return "";
            }

            const auto it = dataAssetPathMap.find(const_cast<Data::DataAsset*>(asset));
            if (it != dataAssetPathMap.end()) {
                return it->second;
            }

            return asset->GetSourcePath();
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

            const std::string resolvedPath = ResolvePathForRead(path);
            ECS::Scene* scene = Scripting::SceneLoader::LoadScene(resolvedPath);
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
            shaderAssetRefs.clear();
            shaderAssetDataCache.clear();
            textures.clear();
            modelMeshPtrs.clear();
            modelMeshes.clear();
            modelDataCache.clear();
            audioClips.clear();
            fonts.clear();
            scenes.clear();
            defaultFont = nullptr;
            cubemaps.clear();
            ClearDataAssets();
            defaultSkybox.reset();

            texturePathMap.clear();
            audioClipPathMap.clear();
            meshPathMap.clear();
            fontPathMap.clear();
            cubemapPathMap.clear();
        }

        void ResourceManager::ClearDataAssets()
        {
            dataAssets.clear();
            dataAssetPathMap.clear();
        }

    }
}
