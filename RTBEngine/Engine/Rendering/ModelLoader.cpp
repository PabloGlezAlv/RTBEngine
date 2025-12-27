#include "ModelLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/config.h>
#include <iostream>
#include <cfloat>
#include <algorithm>
#include <unordered_map>

namespace RTBEngine {
    namespace Rendering {

        Math::Matrix4 ModelLoader::ConvertMatrix(const aiMatrix4x4& from)
        {
            Math::Matrix4 to;
            // Assimp matrices are row-major, we need column-major
            // Matrix4 uses m[16] as 1D array in column-major order
            // Column 0
            to.m[0] = from.a1;
            to.m[1] = from.b1;
            to.m[2] = from.c1;
            to.m[3] = from.d1;
            // Column 1
            to.m[4] = from.a2;
            to.m[5] = from.b2;
            to.m[6] = from.c2;
            to.m[7] = from.d2;
            // Column 2
            to.m[8] = from.a3;
            to.m[9] = from.b3;
            to.m[10] = from.c3;
            to.m[11] = from.d3;
            // Column 3
            to.m[12] = from.a4;
            to.m[13] = from.b4;
            to.m[14] = from.c4;
            to.m[15] = from.d4;
            return to;
        }

        ModelData ModelLoader::LoadModelWithAnimations(const std::string& path)
        {
            ModelData result;
            result.skeleton = std::make_shared<Animation::Skeleton>();

            // Extract model directory for texture path resolution
            size_t lastSlash = path.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                result.modelDirectory = path.substr(0, lastSlash);
            }

            // Use Assimp C++ API to set import properties
            Assimp::Importer importer;

            // CRITICAL: Disable preserve pivots for Mixamo FBX files
            // This prevents Assimp from creating extra _$AssimpFbx$_PreRotation nodes
            // which cause animation/skeleton mismatch issues
            importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

            const aiScene* scene = importer.ReadFile(path,
                aiProcess_Triangulate |
                aiProcess_FlipUVs |
                aiProcess_CalcTangentSpace |
                aiProcess_GenNormals |
                aiProcess_LimitBoneWeights
            );

            if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
                std::cerr << "[ModelLoader] Assimp Error: " << importer.GetErrorString() << std::endl;
                return result;
            }

            std::cout << "[ModelLoader] " << path << " - Meshes: " << scene->mNumMeshes
                      << ", Animations: " << scene->mNumAnimations << std::endl;

            // Store global inverse transform
            result.skeleton->SetGlobalInverseTransform(
                ConvertMatrix(scene->mRootNode->mTransformation).Inverse()
            );

            // Extract materials
            ExtractMaterials(scene, result);

            // Process meshes and extract bones
            ProcessNode(scene->mRootNode, scene, result.meshes, result.skeleton);

            // Build bone hierarchy from node tree
            BuildBoneHierarchy(scene->mRootNode, result.skeleton, -1);

            // Compute local bind transforms from offset matrices
            // (needed because FBX from Mixamo has identity transforms in node tree)
            ComputeLocalBindTransforms(result.skeleton);

            // Process animations
            for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
                auto clip = ProcessAnimation(scene->mAnimations[i]);
                if (clip) {
                    result.animations.push_back(clip);
                }
            }

            // Note: Assimp::Importer automatically releases the scene when it goes out of scope

            // If no bones were found, clear the skeleton
            if (result.skeleton->GetBoneCount() == 0) {
                result.skeleton.reset();
            }
            else {
                // Count bones with valid parents
                int rootBones = 0;
                int linkedBones = 0;
                for (size_t i = 0; i < result.skeleton->GetBoneCount(); i++) {
                    const Animation::Bone* bone = result.skeleton->GetBone(static_cast<int>(i));
                    if (bone->parentIndex == -1) rootBones++;
                    else linkedBones++;
                }
                std::cout << "[ModelLoader] Skeleton: " << result.skeleton->GetBoneCount()
                          << " bones (" << rootBones << " roots, " << linkedBones << " linked)" << std::endl;
            }

            // Log animations
            for (const auto& clip : result.animations) {
                std::cout << "[ModelLoader] Animation: '" << clip->GetName()
                          << "' duration=" << clip->GetDuration() / clip->GetTicksPerSecond() << "s" << std::endl;
            }

            return result;
        }

        std::vector<Mesh*> ModelLoader::LoadModel(const std::string& path)
        {
            ModelData data = LoadModelWithAnimations(path);
            return data.meshes;
        }

        void ModelLoader::ProcessNode(const aiNode* node, const aiScene* scene,
            std::vector<Mesh*>& meshes, std::shared_ptr<Animation::Skeleton>& skeleton)
        {
            for (unsigned int i = 0; i < node->mNumMeshes; i++) {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                meshes.push_back(ProcessMesh(mesh, scene, skeleton));
            }

            for (unsigned int i = 0; i < node->mNumChildren; i++) {
                ProcessNode(node->mChildren[i], scene, meshes, skeleton);
            }
        }

        Mesh* ModelLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene,
            std::shared_ptr<Animation::Skeleton>& skeleton)
        {
            std::vector<Vertex> vertices;
            std::vector<unsigned int> indices;

            // Calculate bounding box
            float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
            float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

            // Extract vertex data
            for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
                Vertex vertex;

                vertex.position.x = mesh->mVertices[i].x;
                vertex.position.y = mesh->mVertices[i].y;
                vertex.position.z = mesh->mVertices[i].z;

                // Update bounding box
                minX = std::min(minX, vertex.position.x);
                minY = std::min(minY, vertex.position.y);
                minZ = std::min(minZ, vertex.position.z);
                maxX = std::max(maxX, vertex.position.x);
                maxY = std::max(maxY, vertex.position.y);
                maxZ = std::max(maxZ, vertex.position.z);

                if (mesh->HasNormals()) {
                    vertex.normal.x = mesh->mNormals[i].x;
                    vertex.normal.y = mesh->mNormals[i].y;
                    vertex.normal.z = mesh->mNormals[i].z;
                }
                else {
                    vertex.normal = Math::Vector3(0.0f, 1.0f, 0.0f);
                }

                if (mesh->mTextureCoords[0]) {
                    vertex.texCoords.x = mesh->mTextureCoords[0][i].x;
                    vertex.texCoords.y = mesh->mTextureCoords[0][i].y;
                }
                else {
                    vertex.texCoords = Math::Vector2(0.0f, 0.0f);
                }

                vertices.push_back(vertex);
            }

            // Extract bone data
            if (mesh->HasBones()) {
                ExtractBoneInfo(mesh, vertices, skeleton);
            }

            // Normalize bone weights
            for (auto& vertex : vertices) {
                vertex.NormalizeBoneWeights();
            }

            // Extract indices
            for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
                aiFace& face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; j++) {
                    indices.push_back(face.mIndices[j]);
                }
            }

            Mesh* resultMesh = new Mesh(vertices, indices);
            resultMesh->SetMaterialIndex(static_cast<int>(mesh->mMaterialIndex));
            return resultMesh;
        }

        void ModelLoader::ExtractBoneInfo(aiMesh* mesh, std::vector<Vertex>& vertices,
            std::shared_ptr<Animation::Skeleton>& skeleton)
        {
            static bool debugBones = true;

            for (unsigned int boneIdx = 0; boneIdx < mesh->mNumBones; boneIdx++) {
                aiBone* assimpBone = mesh->mBones[boneIdx];
                std::string boneName = assimpBone->mName.C_Str();

                int boneId = skeleton->GetBoneIndex(boneName);
                if (boneId == -1) {
                    // New bone, add to skeleton
                    Animation::Bone bone;
                    bone.name = boneName;
                    bone.parentIndex = -1;  // Parent will be set when processing node hierarchy
                    bone.offsetMatrix = ConvertMatrix(assimpBone->mOffsetMatrix);
                    skeleton->AddBone(bone);
                    boneId = static_cast<int>(skeleton->GetBoneCount()) - 1;

                    // Debug: print first 5 bone names
                    if (debugBones && boneId < 5) {
                        std::cout << "[ModelLoader]   Bone[" << boneId << "]: '" << boneName << "'" << std::endl;
                    }
                    if (boneId == 4) debugBones = false;
                }

                // Assign bone weights to vertices
                for (unsigned int w = 0; w < assimpBone->mNumWeights; w++) {
                    unsigned int vertexId = assimpBone->mWeights[w].mVertexId;
                    float weight = assimpBone->mWeights[w].mWeight;

                    if (vertexId < vertices.size()) {
                        vertices[vertexId].AddBoneInfluence(boneId, weight);
                    }
                }
            }
        }

        void ModelLoader::BuildBoneHierarchy(const aiNode* node,
            std::shared_ptr<Animation::Skeleton>& skeleton, int parentIndex)
        {
            std::string nodeName = node->mName.C_Str();

            // Check if this node corresponds to a bone
            int boneIndex = skeleton->GetBoneIndex(nodeName);
            if (boneIndex != -1) {
                // Set the parent of this bone
                Animation::Bone* bone = skeleton->GetBone(boneIndex);
                if (bone) {
                    if (bone->parentIndex == -1) {
                        bone->parentIndex = parentIndex;
                    }
                }
                // This bone becomes the parent for children
                parentIndex = boneIndex;
            }

            // Process children
            for (unsigned int i = 0; i < node->mNumChildren; i++) {
                BuildBoneHierarchy(node->mChildren[i], skeleton, parentIndex);
            }
        }

        void ModelLoader::ComputeLocalBindTransforms(std::shared_ptr<Animation::Skeleton>& skeleton)
        {
            // Compute local bind transforms from offset matrices
            // OffsetMatrix transforms from mesh space to bone space (inverse of global bind pose)
            // So OffsetMatrix^-1 gives us the global bind pose of each bone

            size_t boneCount = skeleton->GetBoneCount();

            for (size_t i = 0; i < boneCount; i++) {
                Animation::Bone* bone = skeleton->GetBone(static_cast<int>(i));
                if (!bone) continue;

                // Get global bind pose from inverse of offset matrix
                Math::Matrix4 globalBindPose = bone->offsetMatrix.Inverse();

                if (bone->parentIndex >= 0) {
                    // Get parent's global bind pose
                    const Animation::Bone* parent = skeleton->GetBone(bone->parentIndex);
                    if (parent) {
                        Math::Matrix4 parentGlobalBindPose = parent->offsetMatrix.Inverse();
                        // Local = ParentGlobalInverse * ChildGlobal
                        bone->localBindTransform = parentGlobalBindPose.Inverse() * globalBindPose;
                    }
                } else {
                    // Root bone: local = global
                    bone->localBindTransform = globalBindPose;
                }
            }
        }

        std::shared_ptr<Animation::AnimationClip> ModelLoader::ProcessAnimation(const aiAnimation* anim)
        {
            std::string name = anim->mName.C_Str();
            if (name.empty()) {
                name = "Animation";
            }

            float duration = static_cast<float>(anim->mDuration);
            float ticksPerSecond = static_cast<float>(anim->mTicksPerSecond);
            if (ticksPerSecond == 0.0f) {
                ticksPerSecond = 25.0f;
            }

            auto clip = std::make_shared<Animation::AnimationClip>(name, duration, ticksPerSecond);

            // Use a map to combine multiple channels for the same bone
            // (FBX via Assimp splits Translation/Rotation/Scale into separate channels)
            std::unordered_map<std::string, Animation::BoneAnimation> boneAnimMap;

            // Debug: print all channel names to see what Assimp gives us
            static bool debugChannels = true;
            if (debugChannels) {
                printf("[ModelLoader] Animation channels:\n");
                for (unsigned int i = 0; i < std::min(anim->mNumChannels, 15u); i++) {
                    printf("  [%d] '%s'\n", i, anim->mChannels[i]->mNodeName.C_Str());
                }
                if (anim->mNumChannels > 15) printf("  ... and %d more\n", anim->mNumChannels - 15);
                debugChannels = false;
            }

            for (unsigned int i = 0; i < anim->mNumChannels; i++) {
                aiNodeAnim* channel = anim->mChannels[i];

                std::string channelName = channel->mNodeName.C_Str();
                std::string originalChannelName = channelName;

                // Clean up Assimp FBX suffixes like "_$AssimpFbx$_Translation" or "_$AssimpFbx$_Rotation"
                size_t assimpSuffix = channelName.find("_$AssimpFbx$_");
                std::string suffixType;
                if (assimpSuffix != std::string::npos) {
                    suffixType = channelName.substr(assimpSuffix + 13); // Get suffix type (Translation, Rotation, PreRotation, etc.)
                    channelName = channelName.substr(0, assimpSuffix);
                }

                // Get or create BoneAnimation for this bone
                Animation::BoneAnimation& boneAnim = boneAnimMap[channelName];
                boneAnim.boneName = channelName;

                // Add position keys (if this channel has meaningful position data)
                if (channel->mNumPositionKeys > 1 ||
                    (channel->mNumPositionKeys == 1 && boneAnim.positionKeys.empty())) {
                    for (unsigned int j = 0; j < channel->mNumPositionKeys; j++) {
                        Animation::VectorKey key;
                        key.time = static_cast<float>(channel->mPositionKeys[j].mTime);
                        key.value.x = channel->mPositionKeys[j].mValue.x;
                        key.value.y = channel->mPositionKeys[j].mValue.y;
                        key.value.z = channel->mPositionKeys[j].mValue.z;
                        boneAnim.positionKeys.push_back(key);
                    }
                }

                // Add rotation keys
                if (channel->mNumRotationKeys > 1 ||
                    (channel->mNumRotationKeys == 1 && boneAnim.rotationKeys.empty())) {
                    for (unsigned int j = 0; j < channel->mNumRotationKeys; j++) {
                        Animation::QuatKey key;
                        key.time = static_cast<float>(channel->mRotationKeys[j].mTime);
                        key.value.w = channel->mRotationKeys[j].mValue.w;
                        key.value.x = channel->mRotationKeys[j].mValue.x;
                        key.value.y = channel->mRotationKeys[j].mValue.y;
                        key.value.z = channel->mRotationKeys[j].mValue.z;
                        key.value.Normalize();  // Ensure quaternion is normalized
                        boneAnim.rotationKeys.push_back(key);
                    }
                }

                // Add scale keys
                if (channel->mNumScalingKeys > 1 ||
                    (channel->mNumScalingKeys == 1 && boneAnim.scaleKeys.empty())) {
                    for (unsigned int j = 0; j < channel->mNumScalingKeys; j++) {
                        Animation::VectorKey key;
                        key.time = static_cast<float>(channel->mScalingKeys[j].mTime);
                        key.value.x = channel->mScalingKeys[j].mValue.x;
                        key.value.y = channel->mScalingKeys[j].mValue.y;
                        key.value.z = channel->mScalingKeys[j].mValue.z;
                        boneAnim.scaleKeys.push_back(key);
                    }
                }
            }

            // Add all combined bone animations to the clip
            for (auto& pair : boneAnimMap) {
                clip->AddBoneAnimation(pair.second);
            }

            std::cout << "[ModelLoader] Animation '" << name << "': " << anim->mNumChannels
                      << " channels -> " << boneAnimMap.size() << " bones" << std::endl;

            return clip;
        }

        std::string ModelLoader::ResolvePath(const std::string& modelDir, const std::string& texPath)
        {
            if (texPath.empty()) {
                return "";
            }

            // Handle absolute paths
            if (texPath.find(':') != std::string::npos || texPath[0] == '/' || texPath[0] == '\\') {
                return texPath;
            }

            // Combine with model directory
            std::string fullPath = modelDir;
            if (!fullPath.empty() && fullPath.back() != '/' && fullPath.back() != '\\') {
                fullPath += '/';
            }
            fullPath += texPath;

            // Normalize path separators to forward slashes
            std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

            return fullPath;
        }

        void ModelLoader::ExtractMaterials(const aiScene* scene, ModelData& outData)
        {
            // First, extract all embedded textures
            for (unsigned int i = 0; i < scene->mNumTextures; i++) {
                aiTexture* tex = scene->mTextures[i];
                EmbeddedTexture embeddedTex;

                if (tex->mHeight == 0) {
                    // Compressed format (PNG, JPG, etc.) - stored as raw bytes
                    embeddedTex.isCompressed = true;
                    embeddedTex.width = tex->mWidth;  // mWidth is the data size in bytes for compressed textures
                    embeddedTex.height = 0;
                    embeddedTex.data.resize(tex->mWidth);
                    memcpy(embeddedTex.data.data(), tex->pcData, tex->mWidth);
                }
                else {
                    // Uncompressed ARGB8888 format
                    embeddedTex.isCompressed = false;
                    embeddedTex.width = tex->mWidth;
                    embeddedTex.height = tex->mHeight;
                    embeddedTex.channels = 4;  // ARGB
                    size_t dataSize = tex->mWidth * tex->mHeight * 4;
                    embeddedTex.data.resize(dataSize);
                    // Convert ARGB to RGBA
                    for (unsigned int p = 0; p < tex->mWidth * tex->mHeight; p++) {
                        aiTexel& texel = tex->pcData[p];
                        embeddedTex.data[p * 4 + 0] = texel.r;
                        embeddedTex.data[p * 4 + 1] = texel.g;
                        embeddedTex.data[p * 4 + 2] = texel.b;
                        embeddedTex.data[p * 4 + 3] = texel.a;
                    }
                }

                outData.embeddedTextures.push_back(std::move(embeddedTex));
            }

            outData.materials.reserve(scene->mNumMaterials);

            for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
                aiMaterial* mat = scene->mMaterials[i];
                LoadedMaterial loadedMat;

                // Get material name
                aiString matName;
                if (mat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
                    loadedMat.name = matName.C_Str();
                }

                // Diffuse color
                aiColor3D diffuse(1.0f, 1.0f, 1.0f);
                if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
                    loadedMat.diffuseColor = Math::Vector3(diffuse.r, diffuse.g, diffuse.b);
                }

                // Opacity
                float opacity = 1.0f;
                if (mat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
                    loadedMat.opacity = opacity;
                }

                // Diffuse texture
                if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                    aiString texPath;
                    if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
                        std::string texPathStr = texPath.C_Str();

                        // Check if it's an embedded texture reference (starts with *)
                        if (!texPathStr.empty() && texPathStr[0] == '*') {
                            // Embedded texture - parse index
                            int embeddedIndex = std::atoi(texPathStr.c_str() + 1);
                            if (embeddedIndex >= 0 && embeddedIndex < static_cast<int>(outData.embeddedTextures.size())) {
                                loadedMat.embeddedTextureIndex = embeddedIndex;
                            }
                        }
                        else {
                            // Try to find embedded texture by path using Assimp's GetEmbeddedTexture
                            const aiTexture* embeddedTex = scene->GetEmbeddedTexture(texPath.C_Str());
                            if (embeddedTex) {
                                // Find the index of this embedded texture
                                for (unsigned int t = 0; t < scene->mNumTextures; t++) {
                                    if (scene->mTextures[t] == embeddedTex) {
                                        loadedMat.embeddedTextureIndex = static_cast<int>(t);
                                        break;
                                    }
                                }
                            }
                            else {
                                // External texture file
                                loadedMat.diffuseTexturePath = ResolvePath(outData.modelDirectory, texPathStr);
                            }
                        }
                    }
                }

                outData.materials.push_back(loadedMat);
            }
        }

    }
}
