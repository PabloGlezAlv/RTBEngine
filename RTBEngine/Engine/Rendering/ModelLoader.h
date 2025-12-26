#pragma once
#include "Mesh.h"
#include "../Animation/Skeleton.h"
#include "../Animation/AnimationClip.h"
#include <assimp/matrix4x4.h>
#include <string>
#include <vector>
#include <memory>

struct aiNode;
struct aiScene;
struct aiMesh;
struct aiAnimation;

namespace RTBEngine {
    namespace Rendering {

        // Result of loading an animated model
        struct ModelData {
            std::vector<Mesh*> meshes;
            std::shared_ptr<Animation::Skeleton> skeleton;
            std::vector<std::shared_ptr<Animation::AnimationClip>> animations;
        };

        class ModelLoader {
        public:
            static ModelData LoadModelWithAnimations(const std::string& path);

            static std::vector<Mesh*> LoadModel(const std::string& path);

        private:
            static void ProcessNode(const aiNode* node, const aiScene* scene,
                std::vector<Mesh*>& meshes, std::shared_ptr<Animation::Skeleton>& skeleton);
            static Mesh* ProcessMesh(aiMesh* mesh, const aiScene* scene,
                std::shared_ptr<Animation::Skeleton>& skeleton);

            // Animation extraction
            static void ExtractBoneInfo(aiMesh* mesh, std::vector<Vertex>& vertices,
                std::shared_ptr<Animation::Skeleton>& skeleton);
            static std::shared_ptr<Animation::AnimationClip> ProcessAnimation(
                const aiAnimation* anim);

            // Helper to convert Assimp matrix to engine matrix
            static Math::Matrix4 ConvertMatrix(const aiMatrix4x4& from);
        };

    }
}
