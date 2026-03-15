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

namespace RTBEngine {
    namespace ECS {
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
            std::vector<Material*> meshMaterials;          // Per-mesh materials (non-owned)
            std::vector<Texture*> embeddedTextureObjects;  // Textures created from embedded data (owned elsewhere)
        };

        // Build meshes and per-mesh materials from ModelData.
        // Reuses materials by FBX material name and uses embedded or external textures.
        inline FbxBindingResult BuildMeshesAndMaterials(const FbxBindingContext& ctx)
        {
            FbxBindingResult result;
            result.meshes = ctx.modelData.meshes;

            // Build embedded textures once
            std::vector<Texture*> embeddedTextures;
            embeddedTextures.reserve(ctx.modelData.embeddedTextures.size());

            for (const EmbeddedTexture& embTex : ctx.modelData.embeddedTextures) {
                Texture* tex = new Texture();
                bool loaded = false;

                if (embTex.isCompressed) {
                    loaded = tex->LoadFromCompressedMemory(
                        embTex.data.data(),
                        static_cast<int>(embTex.data.size()));
                }
                else {
                    loaded = tex->LoadFromMemory(
                        embTex.data.data(),
                        embTex.width,
                        embTex.height,
                        embTex.channels);
                }

                if (loaded) {
                    embeddedTextures.push_back(tex);
                }
                else {
                    RTB_WARN("[FbxBinding] Failed to load embedded texture for model: " + ctx.modelPath);
                    embeddedTextures.push_back(nullptr);
                    delete tex;
                }
            }

            result.embeddedTextureObjects = embeddedTextures;

            // Cache materials by FBX material name
            std::unordered_map<std::string, Material*> materialCache;
            result.meshMaterials.reserve(ctx.modelData.meshes.size());

            // Shader will be set by caller; we just default to nullptr here
            Shader* defaultShader = nullptr;

            for (Mesh* mesh : ctx.modelData.meshes) {
                if (!mesh) {
                    result.meshMaterials.push_back(nullptr);
                    continue;
                }

                const int matIdx = mesh->GetMaterialIndex();
                if (matIdx < 0 || matIdx >= static_cast<int>(ctx.modelData.materials.size())) {
                    result.meshMaterials.push_back(nullptr);
                    continue;
                }

                const LoadedMaterial& loadedMat = ctx.modelData.materials[matIdx];

                Material* mat = nullptr;

                if (!loadedMat.name.empty()) {
                    auto it = materialCache.find(loadedMat.name);
                    if (it != materialCache.end()) {
                        mat = it->second;
                    }
                }

                if (!mat) {
                    mat = new Material(defaultShader);

                    bool hasTexture = false;

                    if (loadedMat.embeddedTextureIndex >= 0 &&
                        loadedMat.embeddedTextureIndex < static_cast<int>(embeddedTextures.size()) &&
                        embeddedTextures[loadedMat.embeddedTextureIndex]) {
                        mat->SetTexture(embeddedTextures[loadedMat.embeddedTextureIndex]);
                        hasTexture = true;
                    }
                    else if (!loadedMat.diffuseTexturePath.empty()) {
                        Texture* tex = ctx.resources.LoadTexture(loadedMat.diffuseTexturePath);
                        if (tex) {
                            mat->SetTexture(tex);
                            hasTexture = true;
                        }
                        else {
                            RTB_WARN("[FbxBinding] Failed to load diffuse texture from FBX path: " +
                                     loadedMat.diffuseTexturePath);
                        }
                    }
                    else {
                        RTB_WARN("[FbxBinding] FBX material '" + loadedMat.name +
                                 "' has no diffuse texture or embedded texture");
                    }

                    // When a diffuse texture is present, force diffuse color to white so
                    // the FBX material color does not darken/tint the texture.
                    // FBX exporters often store a non-white diffuse color alongside the
                    // texture which causes muddy/dark rendering when multiplied in the shader.
                    if (hasTexture) {
                        mat->SetDiffuseColor(Math::Vector3(1.0f, 1.0f, 1.0f));
                    }
                    else {
                        mat->SetDiffuseColor(loadedMat.diffuseColor);
                    }

                    if (!loadedMat.name.empty()) {
                        materialCache[loadedMat.name] = mat;
                    }
                }

                result.meshMaterials.push_back(mat);
            }

            return result;
        }

        // Build a root+children hierarchy in the scene from a multi-mesh ModelData.
        // Root GameObject holds the Animator (if the FBX has animation data).
        // Each mesh gets its own child GameObject with a MeshRenderer.
        // Declared here; implemented in FbxBinding.cpp.
        RTB_API ECS::GameObject* BuildFbxHierarchy(
            ECS::Scene* scene,
            const ModelData& modelData,
            const std::string& fbxPath,
            Core::ResourceManager& resources);

    }
}


