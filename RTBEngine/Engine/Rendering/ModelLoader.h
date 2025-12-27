#pragma once
#include "Mesh.h"
#include "../Animation/Skeleton.h"
#include "../Animation/AnimationClip.h"
#include "../Math/Vectors/Vector3.h"
#include <assimp/matrix4x4.h>
#include <string>
#include <vector>
#include <memory>

struct aiNode;
struct aiScene;
struct aiMesh;
struct aiAnimation;
struct aiMaterial;

namespace RTBEngine {
    namespace Rendering {

        // Embedded texture data
        struct EmbeddedTexture {
            std::vector<unsigned char> data;
            int width = 0;
            int height = 0;
            int channels = 0;
            bool isCompressed = false;  // If true, data is PNG/JPG format
        };

        // Material data extracted from model file
        struct LoadedMaterial {
            std::string name;
            Math::Vector3 diffuseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            std::string diffuseTexturePath;
            int embeddedTextureIndex = -1;  // Index into ModelData::embeddedTextures
            float opacity = 1.0f;
        };

        // Result of loading an animated model
        struct ModelData {
            std::vector<Mesh*> meshes;
            std::shared_ptr<Animation::Skeleton> skeleton;
            std::vector<std::shared_ptr<Animation::AnimationClip>> animations;
            std::vector<LoadedMaterial> materials;
            std::vector<EmbeddedTexture> embeddedTextures;
            std::string modelDirectory;
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
            static void BuildBoneHierarchy(const aiNode* node,
                std::shared_ptr<Animation::Skeleton>& skeleton, int parentIndex);
            static void ComputeLocalBindTransforms(std::shared_ptr<Animation::Skeleton>& skeleton);

            // Helper to convert Assimp matrix to engine matrix
            static Math::Matrix4 ConvertMatrix(const aiMatrix4x4& from);

            // Material extraction
            static void ExtractMaterials(const aiScene* scene, ModelData& outData);
            static std::string ResolvePath(const std::string& modelDir, const std::string& texPath);
        };

    }
}
