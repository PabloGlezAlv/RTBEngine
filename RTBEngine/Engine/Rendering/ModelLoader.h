#pragma once
#include "Mesh.h"
#include <string>
#include <vector>

struct aiNode;
struct aiScene;
struct aiMesh;

namespace RTBEngine {
    namespace Rendering {

        class ModelLoader {
        public:
            static std::vector<Mesh*> LoadModel(const std::string& path);

        private:
            static void ProcessNode(const aiNode* node, const aiScene* scene, std::vector<Mesh*>& meshes);
            static Mesh* ProcessMesh(aiMesh* mesh, const aiScene* scene);
        };

    }
}