#pragma once
#include "../Rendering/Shader.h"
#include "../Rendering/Texture.h"
#include "../Rendering/Mesh.h"
#include "../Rendering/ModelLoader.h"
#include "../Audio/AudioClip.h"
#include <unordered_map>
#include <string>
#include <memory>

namespace RTBEngine {
    namespace Core {

        class ResourceManager {
        public:
            static ResourceManager& GetInstance();

            ResourceManager(const ResourceManager&) = delete;
            ResourceManager& operator=(const ResourceManager&) = delete;

            // Shader management
            Rendering::Shader* GetShader(const std::string& name);
            Rendering::Shader* LoadShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath);

            // Texture management
            Rendering::Texture* GetTexture(const std::string& path);
            Rendering::Texture* LoadTexture(const std::string& path);

			// Model management
            Rendering::Mesh* GetModel(const std::string& path);
            Rendering::Mesh* LoadModel(const std::string& path);

            // Audio management
            Audio::AudioClip* GetAudioClip(const std::string& path);
            Audio::AudioClip* LoadAudioClip(const std::string& path, bool stream = false);

            void Clear();

        private:
            ResourceManager() = default;
            ~ResourceManager();

            std::unordered_map<std::string, std::unique_ptr<Rendering::Shader>> shaders;
            std::unordered_map<std::string, std::unique_ptr<Rendering::Texture>> textures;
            std::unordered_map<std::string, std::unique_ptr<Rendering::Mesh>> models;
            std::unordered_map<std::string, std::unique_ptr<Audio::AudioClip>> audioClips;

        };

    }
}