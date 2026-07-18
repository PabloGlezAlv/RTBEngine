#pragma once
#include "../RTBEngineAPI.h"
#include "ModelLoader.h"
#include "Material.h"
#include "Texture.h"
#include "../Core/ResourceManager.h"
#include "../RTBEngine.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <filesystem>
#include <memory>

namespace RTBEngine {
    namespace Scene {
        class Scene;
        class GameObject;
    }
}

namespace RTBEngine {
    namespace Rendering {

        struct FbxBindingContext {
            Core::ResourceManager& resources;
            std::string modelPath;
            const ModelData& modelData;
        };

        struct FbxBindingResult {
            std::vector<Mesh*> meshes;                     // Mesh list (usually modelData.meshes)
            std::vector<Material*> meshMaterials;          // Per-mesh materials (non-owning views)
            std::vector<std::unique_ptr<Material>> ownedMaterials; // Lifetime storage for meshMaterials
            std::vector<Texture*> embeddedTextureObjects;  // Textures created from embedded data (owned elsewhere)
        };

        RTB_API Texture* LoadExternalTexture(const FbxBindingContext& ctx, const std::string& path);

        // Build meshes and per-mesh materials from ModelData.
        // Reuses materials by FBX material name and uses embedded or external textures.
        RTB_API FbxBindingResult BuildMeshesAndMaterials(const FbxBindingContext& ctx);

        // Attach mesh renderers for all meshes in modelData under an existing root GO.
        RTB_API void AttachFbxMeshesToHierarchy(
            Scene::Scene* scene,
            Scene::GameObject* rootGO,
            const ModelData& modelData,
            const FbxBindingResult& binding,
            Shader* basicShader);

        // Build a root+children hierarchy in the scene from a multi-mesh ModelData.
        // Root GameObject holds the Animator (if the FBX has animation data).
        // Each mesh gets its own child GameObject with a MeshRenderer.
        RTB_API Scene::GameObject* BuildFbxHierarchy(
            Scene::Scene* scene,
            const ModelData& modelData,
            const std::string& fbxPath,
            Core::ResourceManager& resources);

    }
}
