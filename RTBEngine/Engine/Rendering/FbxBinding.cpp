#include "FbxBinding.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/MeshRenderer.h"
#include "../Animation/Animator.h"
#include "../Core/Logger.h"
#include <filesystem>

namespace RTBEngine {
    namespace Rendering {

        ECS::GameObject* BuildFbxHierarchy(
            ECS::Scene* scene,
            const ModelData& modelData,
            const std::string& fbxPath,
            Core::ResourceManager& resources)
        {
            if (!scene) return nullptr;
            if (modelData.meshes.empty()) return nullptr;

            // Derive a clean stem name from the file path
            std::string stem = std::filesystem::path(fbxPath).stem().string();

            // Build per-mesh materials once
            FbxBindingContext ctx{ resources, fbxPath, modelData };
            FbxBindingResult binding = BuildMeshesAndMaterials(ctx);

            // Get the basic shader for applying to materials that have none
            Rendering::Shader* basicShader = resources.GetShader("basic");

            for (Rendering::Material* mat : binding.meshMaterials) {
                if (mat && !mat->GetShader() && basicShader) {
                    mat->SetShader(basicShader);
                }
            }

            // Create root GameObject
            ECS::GameObject* root = new ECS::GameObject(stem);
            scene->AddGameObject(root);

            // Add Animator to root if the FBX has animation data
            if (modelData.skeleton || !modelData.animations.empty()) {
                auto* animator = new Animation::Animator();
                if (modelData.skeleton) {
                    animator->SetSkeleton(modelData.skeleton);
                }
                for (const auto& clip : modelData.animations) {
                    animator->AddClip(clip->GetName(), clip);
                }
                if (!modelData.animations.empty()) {
                    animator->defaultClip = modelData.animations[0]->GetName();
                }
                animator->modelRef = fbxPath;
                root->AddComponent(animator);

                // Create bone GameObjects from skeleton hierarchy
                animator->CreateBoneGameObjects(scene);
            }

            // Create a single MeshRenderer on root for all meshes
            auto* renderer = new ECS::MeshRenderer();

            if (modelData.meshes.size() > 1) {
                // Multi-mesh mode
                renderer->SetMeshes(modelData.meshes);
                for (size_t i = 0; i < binding.meshMaterials.size() && i < modelData.meshes.size(); i++) {
                    if (binding.meshMaterials[i]) {
                        renderer->SetMaterialForMesh(static_cast<int>(i), binding.meshMaterials[i]);
                    }
                }
            }
            else {
                // Single-mesh mode
                renderer->meshIndex = 0;
                renderer->SetMesh(modelData.meshes[0]);
                if (!binding.meshMaterials.empty() && binding.meshMaterials[0]) {
                    renderer->SetMaterial(binding.meshMaterials[0]);
                }
                else if (basicShader) {
                    renderer->SetShader(basicShader);
                }
            }

            root->AddComponent(renderer);

            return root;
        }

    }
}
