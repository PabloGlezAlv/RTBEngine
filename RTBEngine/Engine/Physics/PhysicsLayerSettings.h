#pragma once

#include "../RTBEngineAPI.h"

#include <filesystem>
#include <string>
#include <vector>

class btCollisionObject;

namespace RTBEngine {
    namespace Scene {
        class GameObject;
    }

    namespace Physics {

        class RTB_API PhysicsLayerSettings {
        public:
            static constexpr int MaxLayers = 32;

            static PhysicsLayerSettings& Get();

            void ResetToDefaults();
            bool LoadFromFile(const std::filesystem::path& path);
            bool SaveToFile(const std::filesystem::path& path) const;

            int GetLayerCount() const { return layerCount; }
            void SetLayerCount(int count);

            const std::string& GetLayerName(int layerIndex) const;
            void SetLayerName(int layerIndex, const std::string& name);

            int GetLayerIndex(const std::string& name) const;
            bool GetLayerCollision(int layerA, int layerB) const;
            void SetLayerCollision(int layerA, int layerB, bool collides);

            int GetLayerGroup(int layerIndex) const;
            int GetLayerMask(int layerIndex) const;

            void ApplyToCollisionObject(::btCollisionObject* object, int layerIndex) const;
            void ApplyToGameObject(Scene::GameObject* gameObject) const;

            static std::filesystem::path GetDefaultSettingsFileName();

        private:
            PhysicsLayerSettings();

            void EnsureLayerCountInRange();
            void ClampLayerIndex(int& layerIndex) const;

            int layerCount = 1;
            std::vector<std::string> layerNames;
            bool collisionMatrix[MaxLayers][MaxLayers] = {};
        };

        inline constexpr int kMaxPhysicsLayers = PhysicsLayerSettings::MaxLayers;

    }
}
