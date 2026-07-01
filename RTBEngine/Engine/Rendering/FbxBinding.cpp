#include "FbxBinding.h"
#include "../Scene/Scene.h"
#include "../Scene/GameObject.h"
#include "../Scene/MeshRenderer.h"
#include "../Animation/Animator.h"
#include "../Core/Logger.h"
#include <filesystem>

namespace RTBEngine {
    namespace Rendering {

        Texture* LoadExternalTexture(const FbxBindingContext& ctx, const std::string& path)
        {
            if (path.empty()) {
                return nullptr;
            }

            const std::string modelAssetPath = !ctx.modelData.modelAssetPath.empty()
                ? ctx.modelData.modelAssetPath
                : ctx.modelPath;

            const std::string resolvedPath = ModelLoader::ResolveExternalTexturePath(
                ctx.modelData.modelDirectory, modelAssetPath, path);

            if (resolvedPath.empty()) {
                return nullptr;
            }

            // UVs are already converted to OpenGL space by aiProcess_FlipUVs in ModelLoader.
            // Do not flip the image again (double-flip misaligns palette/atlas textures).
            return ctx.resources.LoadModelTexture(resolvedPath);
        }

        FbxBindingResult BuildMeshesAndMaterials(const FbxBindingContext& ctx)
        {
            FbxBindingResult result;
            result.meshes = ctx.modelData.meshes;

            const std::string resolvedModelPath = ctx.resources.ResolvePathForRead(ctx.modelPath);
            const std::filesystem::path resolvedModelFile(resolvedModelPath);
            const std::string fbxStem = resolvedModelFile.stem().string();
            const std::filesystem::path fbxDir = resolvedModelFile.parent_path();

            std::vector<std::string> texNames(ctx.modelData.embeddedTextures.size());
            for (size_t i = 0; i < ctx.modelData.materials.size(); i++) {
                int idx = ctx.modelData.materials[i].embeddedTextureIndex;
                if (idx < 0 || idx >= static_cast<int>(texNames.size())) continue;
                if (!texNames[idx].empty()) continue;
                std::string matName = ctx.modelData.materials[i].name;
                for (char& c : matName)
                    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
                texNames[idx] = matName.empty() ? ("tex_" + std::to_string(idx)) : matName;
            }

            std::vector<Texture*> embeddedTextures;
            embeddedTextures.reserve(ctx.modelData.embeddedTextures.size());

            for (size_t i = 0; i < ctx.modelData.embeddedTextures.size(); i++) {
                const EmbeddedTexture& embTex = ctx.modelData.embeddedTextures[i];
                std::string name = texNames[i].empty()
                    ? (fbxStem + "_tex" + std::to_string(i))
                    : (fbxStem + "_" + texNames[i]);

                Texture* tex = nullptr;
                const std::string textureAssetPath = (fbxDir / (name + ".texture")).string();
                if (std::filesystem::exists(ctx.resources.ResolvePathForRead(textureAssetPath))) {
                    tex = ctx.resources.LoadTextureAsset(textureAssetPath);
                }

                if (!tex) {
                    for (const char* ext : { ".png", ".jpg" }) {
                        std::filesystem::path candidateTextureAssetPath = fbxDir / (name + ext);
                        candidateTextureAssetPath.replace_extension(".texture");
                        const std::string candidateTextureAssetPathString = candidateTextureAssetPath.string();
                        if (std::filesystem::exists(
                                ctx.resources.ResolvePathForRead(candidateTextureAssetPathString))) {
                            tex = ctx.resources.LoadTextureAsset(candidateTextureAssetPathString);
                            if (tex) break;
                        }
                    }
                }

                if (!tex) {
                    tex = new Texture();
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
                        std::string syntheticPath = ctx.modelPath + "#" + name;
                        ctx.resources.RegisterTexture(syntheticPath, tex);
                    }
                    else {
                        RTB_WARN("[FbxBinding] Failed to load embedded texture for model: " + ctx.modelPath);
                        delete tex;
                        tex = nullptr;
                    }
                }

                embeddedTextures.push_back(tex);
            }

            result.embeddedTextureObjects = embeddedTextures;

            std::unordered_map<int, Material*> materialCache;
            result.meshMaterials.reserve(ctx.modelData.meshes.size());
            result.ownedMaterials.reserve(ctx.modelData.materials.size());

            Shader* defaultShader = nullptr;

            auto applyLoadedMaterialTexture = [&](Material* mat, const LoadedMaterial& loadedMat) -> bool {
                if (!mat) {
                    return false;
                }

                bool hasTexture = false;

                if (loadedMat.embeddedTextureIndex >= 0 &&
                    loadedMat.embeddedTextureIndex < static_cast<int>(embeddedTextures.size()) &&
                    embeddedTextures[loadedMat.embeddedTextureIndex]) {
                    mat->SetTexture(embeddedTextures[loadedMat.embeddedTextureIndex]);
                    hasTexture = true;
                }
                else if (!loadedMat.diffuseTexturePath.empty()) {
                    Texture* tex = LoadExternalTexture(ctx, loadedMat.diffuseTexturePath);
                    if (tex) {
                        mat->SetTexture(tex);
                        hasTexture = true;
                    }
                    else {
                        RTB_WARN("[FbxBinding] Failed to load diffuse texture for model '" +
                                 ctx.modelPath + "'. FBX path: " + loadedMat.diffuseTexturePath);
                    }
                }

                if (hasTexture) {
                    mat->SetDiffuseColor(Math::Vector3(1.0f, 1.0f, 1.0f));
                }
                else if (!mat->GetTexture()) {
                    mat->SetDiffuseColor(loadedMat.diffuseColor);
                }

                return hasTexture;
            };

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
                auto cacheIt = materialCache.find(matIdx);
                if (cacheIt != materialCache.end()) {
                    mat = cacheIt->second;
                }

                if (!mat) {
                    result.ownedMaterials.push_back(std::make_unique<Material>(defaultShader));
                    mat = result.ownedMaterials.back().get();

                    if (!applyLoadedMaterialTexture(mat, loadedMat)) {
                        const std::string label = loadedMat.name.empty()
                            ? "FBX material"
                            : ("FBX material '" + loadedMat.name + "'");
                        RTB_WARN("[FbxBinding] " + label + " has no diffuse texture or embedded texture");
                    }

                    materialCache[matIdx] = mat;
                }

                result.meshMaterials.push_back(mat);
            }

            return result;
        }

        static void ApplyNodeTransform(ECS::GameObject* go, const NodeData* node)
        {
            if (!go || !node) {
                return;
            }

            Math::Vector3 position;
            Math::Quaternion rotation;
            Math::Vector3 scale;
            node->localTransform.Decompose(position, rotation, scale);

            go->GetTransform().SetPosition(position);
            go->GetTransform().SetRotation(rotation);
            go->GetTransform().SetScale(scale);
        }

        static void AddMeshRendererToGO(
            ECS::GameObject* go,
            int meshIdx,
            const ModelData& modelData,
            const FbxBindingResult& binding,
            Rendering::Shader* basicShader,
            const std::string& fbxPath)
        {
            if (meshIdx < 0 || meshIdx >= static_cast<int>(modelData.meshes.size()))
                return;

            auto* renderer = new ECS::MeshRenderer();
            renderer->meshIndex = meshIdx;
            renderer->SetMesh(modelData.meshes[meshIdx]);

            if (meshIdx < static_cast<int>(binding.meshMaterials.size()) && binding.meshMaterials[meshIdx]) {
                if (basicShader) binding.meshMaterials[meshIdx]->SetShader(basicShader);
                renderer->SetMaterial(binding.meshMaterials[meshIdx]);
            } else if (basicShader) {
                renderer->SetShader(basicShader);
            }

            go->AddComponent(renderer);
        }

        static void BuildNodeHierarchy(
            const NodeData* node,
            ECS::GameObject* parentGO,
            ECS::Scene* scene,
            const ModelData& modelData,
            const FbxBindingResult& binding,
            Rendering::Shader* basicShader,
            const std::string& fbxPath)
        {
            for (const auto& child : node->children) {
                std::string childName = child->name.empty() ? "Node" : child->name;
                auto* childGO = new ECS::GameObject(childName);
                scene->AddGameObject(childGO);
                childGO->SetParent(parentGO);
                ApplyNodeTransform(childGO, child.get());

                // Add MeshRenderer for the first mesh on this node
                if (!child->meshIndices.empty()) {
                    AddMeshRendererToGO(childGO, child->meshIndices[0],
                        modelData, binding, basicShader, fbxPath);

                    // Extra meshes on same node get their own child GOs
                    for (size_t m = 1; m < child->meshIndices.size(); m++) {
                        int idx = child->meshIndices[m];
                        std::string extraName = childName + "_Mesh" + std::to_string(m);
                        auto* extraGO = new ECS::GameObject(extraName);
                        scene->AddGameObject(extraGO);
                        extraGO->SetParent(childGO);
                        AddMeshRendererToGO(extraGO, idx, modelData, binding, basicShader, fbxPath);
                    }
                }

                BuildNodeHierarchy(child.get(), childGO, scene, modelData, binding, basicShader, fbxPath);
            }
        }

        void AttachFbxMeshesToHierarchy(
            ECS::Scene* scene,
            ECS::GameObject* rootGO,
            const ModelData& modelData,
            const FbxBindingResult& binding,
            Rendering::Shader* basicShader)
        {
            if (!scene || !rootGO || modelData.meshes.empty()) {
                return;
            }

            for (Rendering::Material* mat : binding.meshMaterials) {
                if (mat && !mat->GetShader() && basicShader) {
                    mat->SetShader(basicShader);
                }
            }

            if (modelData.meshes.size() == 1) {
                AddMeshRendererToGO(rootGO, 0, modelData, binding, basicShader, "");
                return;
            }

            if (!modelData.rootNode) {
                return;
            }

            for (int meshIdx : modelData.rootNode->meshIndices) {
                AddMeshRendererToGO(rootGO, meshIdx, modelData, binding, basicShader, "");
            }

            BuildNodeHierarchy(modelData.rootNode.get(), rootGO, scene, modelData, binding, basicShader, "");
        }

        ECS::GameObject* BuildFbxHierarchy(
            ECS::Scene* scene,
            const ModelData& modelData,
            const std::string& fbxPath,
            Core::ResourceManager& resources)
        {
            if (!scene) return nullptr;
            if (modelData.meshes.empty()) return nullptr;

            std::string stem = std::filesystem::path(fbxPath).stem().string();

            resources.RegisterMeshes(fbxPath, modelData.meshes);

            FbxBindingContext ctx{ resources, fbxPath, modelData };
            FbxBindingResult binding = BuildMeshesAndMaterials(ctx);

            Rendering::Shader* basicShader = resources.GetShader("basic");

            ECS::GameObject* root = new ECS::GameObject(stem);
            scene->AddGameObject(root);
            if (modelData.rootNode) {
                ApplyNodeTransform(root, modelData.rootNode.get());
            }

            if (modelData.skeleton || !modelData.animations.empty()) {
                auto* animator = new Animation::Animator();
                if (modelData.skeleton) {
                    animator->SetSkeleton(modelData.skeleton);
                }
                for (const auto& clip : modelData.animations) {
                    animator->AddClip(clip->GetName(), clip);
                }
                if (!modelData.animations.empty()) {
                    animator->defaultClip = Animation::Animator::NormalizeClipName(
                        modelData.animations[0]->GetName());
                }
                animator->modelRef = fbxPath;
                root->AddComponent(animator);
                animator->CreateBoneGameObjects(scene);
            }

            AttachFbxMeshesToHierarchy(scene, root, modelData, binding, basicShader);

            return root;
        }

    }
}
