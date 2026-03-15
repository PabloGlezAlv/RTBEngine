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
            }

            // Create one child per mesh
            for (size_t i = 0; i < modelData.meshes.size(); i++) {
                Rendering::Mesh* mesh = modelData.meshes[i];
                if (!mesh) continue;

                // Derive child name
                std::string childName;
                if (i < modelData.meshNames.size() && !modelData.meshNames[i].empty()) {
                    childName = modelData.meshNames[i];
                }
                else {
                    RTB_WARN("[FbxBinding] Mesh node " + std::to_string(i) + " in '" + fbxPath + "' has no name, using fallback.");
                    childName = stem + "_mesh_" + std::to_string(i);
                }

                ECS::GameObject* child = new ECS::GameObject(childName);
                child->SetParent(root);
                scene->AddGameObject(child);

                auto* renderer = new ECS::MeshRenderer();
                renderer->meshIndex = static_cast<int>(i);
                renderer->SetMesh(mesh);

                // Apply per-mesh material
                if (i < binding.meshMaterials.size() && binding.meshMaterials[i]) {
                    renderer->SetMaterial(binding.meshMaterials[i]);
                }
                else if (basicShader) {
                    renderer->SetShader(basicShader);
                }

                child->AddComponent(renderer);
            }

            return root;
        }

    }
}
