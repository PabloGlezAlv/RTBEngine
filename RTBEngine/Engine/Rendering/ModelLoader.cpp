#include "ModelLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>

namespace RTBEngine {
    namespace Rendering {

        std::vector<Mesh*> ModelLoader::LoadModel(const std::string& path)
        {
            std::vector<Mesh*> meshes;

            Assimp::Importer importer;

            const aiScene* scene = importer.ReadFile(path.c_str(),
                aiProcess_Triangulate |
				aiProcess_FlipUVs | // Flip UVs for OpenGL coordinate system
                aiProcess_CalcTangentSpace |
                aiProcess_GenNormals
            );

            if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
                std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
                return meshes;
            }

            ProcessNode(scene->mRootNode, scene, meshes);

            return meshes;
        }

        void ModelLoader::ProcessNode(const aiNode* node, const aiScene* scene, std::vector<Mesh*>& meshes)
        {
            // Process all meshes in the current node
            for (unsigned int i = 0; i < node->mNumMeshes; i++) {
                // Get mesh from scene using the index stored in the node
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                meshes.push_back(ProcessMesh(mesh, scene));
            }

            // Recursively process all child nodes
            for (unsigned int i = 0; i < node->mNumChildren; i++) {
                ProcessNode(node->mChildren[i], scene, meshes);
            }
        }

        Mesh* ModelLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene)
        {
            std::vector<Vertex> vertices;
            std::vector<unsigned int> indices;

            // Extract vertex data from Assimp mesh
            for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
                Vertex vertex;

                // Position 
                vertex.position.x = mesh->mVertices[i].x;
                vertex.position.y = mesh->mVertices[i].y;
                vertex.position.z = mesh->mVertices[i].z;

                // Normal 
                if (mesh->HasNormals()) {
                    vertex.normal.x = mesh->mNormals[i].x;
                    vertex.normal.y = mesh->mNormals[i].y;
                    vertex.normal.z = mesh->mNormals[i].z;
                }
                else {
                    vertex.normal.x = 0.0f;
                    vertex.normal.y = 1.0f;
                    vertex.normal.z = 0.0f;
                }

                // Texture coordinates 
                if (mesh->mTextureCoords[0]) {
                    vertex.texCoords.x = mesh->mTextureCoords[0][i].x;
                    vertex.texCoords.y = mesh->mTextureCoords[0][i].y;
                }
                else {
                    vertex.texCoords.x = 0.0f;
                    vertex.texCoords.y = 0.0f;
                }

                vertices.push_back(vertex);
            }

            // Extract indices from faces
            for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
                aiFace face = mesh->mFaces[i];

                for (unsigned int j = 0; j < face.mNumIndices; j++) {
                    indices.push_back(face.mIndices[j]);
                }
            }

            return new Mesh(vertices, indices);
        }

    }
}