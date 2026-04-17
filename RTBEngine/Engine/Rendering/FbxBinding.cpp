#include "FbxBinding.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/MeshRenderer.h"
#include "../Animation/Animator.h"
#include "../Core/Logger.h"
#include <filesystem>

namespace RTBEngine {
    namespace Rendering {

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

            for (Rendering::Material* mat : binding.meshMaterials) {
                if (mat && !mat->GetShader() && basicShader) {
                    mat->SetShader(basicShader);
                }
            }

            //Root GameObject
            ECS::GameObject* root = new ECS::GameObject(stem);
            scene->AddGameObject(root);
            if (modelData.rootNode) {
                ApplyNodeTransform(root, modelData.rootNode.get());
            }

            //Animator on root
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
                animator->CreateBoneGameObjects(scene);
            }

            //Single-mesh FBX: put MeshRenderer on root directly
            if (modelData.meshes.size() == 1) {
                AddMeshRendererToGO(root, 0, modelData, binding, basicShader, fbxPath);
                return root;
            }

            //Multi-mesh FBX: build full node hierarchy
            if (modelData.rootNode) {
                // Meshes on root node itself
                for (int meshIdx : modelData.rootNode->meshIndices) {
                    AddMeshRendererToGO(root, meshIdx, modelData, binding, basicShader, fbxPath);
                }

                BuildNodeHierarchy(modelData.rootNode.get(), root, scene, modelData, binding, basicShader, fbxPath);
            }

            return root;
        }

    }
}
