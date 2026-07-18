#include "PhysicsLayerSettings.h"

#include "../Core/Logger.h"
#include "../Scene/BoxColliderComponent.h"
#include "../Scene/CapsuleColliderComponent.h"
#include "../Scene/GameObject.h"
#include "../Scene/RigidBodyComponent.h"
#include "../Scene/SphereColliderComponent.h"
#include "../Physics/RigidBody.h"

#include <BulletCollision/CollisionDispatch/btCollisionObject.h>
#include <BulletCollision/BroadphaseCollision/btBroadphaseProxy.h>

#include <algorithm>
#include <cctype>
#include <fstream>

namespace RTBEngine {
    namespace Physics {

        namespace {

            std::string Trim(const std::string& value)
            {
                const auto first = std::find_if_not(value.begin(), value.end(),
                    [](unsigned char character) {
                        return std::isspace(character) != 0;
                    });

                if (first == value.end()) {
                    return {};
                }

                const auto last = std::find_if_not(value.rbegin(), value.rend(),
                    [](unsigned char character) {
                        return std::isspace(character) != 0;
                    }).base();

                return std::string(first, last);
            }

            std::string ToLower(std::string value)
            {
                for (char& character : value) {
                    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                }
                return value;
            }

        }

        PhysicsLayerSettings& PhysicsLayerSettings::Get()
        {
            static PhysicsLayerSettings instance;
            return instance;
        }

        PhysicsLayerSettings::PhysicsLayerSettings()
        {
            ResetToDefaults();
        }

        std::filesystem::path PhysicsLayerSettings::GetDefaultSettingsFileName()
        {
            return std::filesystem::path("physics_layers.ini");
        }

        void PhysicsLayerSettings::ResetToDefaults()
        {
            // Engine baseline: a single layer. Game-specific layers live in the project's physics_layers.ini.
            layerCount = 1;
            layerNames.assign(kMaxPhysicsLayers, std::string());
            layerNames[0] = "Default";

            for (int row = 0; row < kMaxPhysicsLayers; ++row) {
                for (int column = 0; column < kMaxPhysicsLayers; ++column) {
                    collisionMatrix[row][column] = false;
                }
            }

            collisionMatrix[0][0] = true;
        }

        void PhysicsLayerSettings::EnsureLayerCountInRange()
        {
            layerCount = std::clamp(layerCount, 1, kMaxPhysicsLayers);
            if (static_cast<int>(layerNames.size()) < kMaxPhysicsLayers) {
                layerNames.resize(kMaxPhysicsLayers);
            }

            for (int i = 0; i < layerCount; ++i) {
                if (layerNames[static_cast<size_t>(i)].empty()) {
                    layerNames[static_cast<size_t>(i)] = "Layer " + std::to_string(i);
                }
            }
        }

        void PhysicsLayerSettings::SetLayerCount(int count)
        {
            layerCount = std::clamp(count, 1, kMaxPhysicsLayers);
            EnsureLayerCountInRange();
        }

        void PhysicsLayerSettings::ClampLayerIndex(int& layerIndex) const
        {
            layerIndex = std::clamp(layerIndex, 0, std::max(0, layerCount - 1));
        }

        const std::string& PhysicsLayerSettings::GetLayerName(int layerIndex) const
        {
            static const std::string kEmpty;
            if (layerIndex < 0 || layerIndex >= kMaxPhysicsLayers) {
                return kEmpty;
            }

            return layerNames[static_cast<size_t>(layerIndex)];
        }

        void PhysicsLayerSettings::SetLayerName(int layerIndex, const std::string& name)
        {
            if (layerIndex < 0 || layerIndex >= kMaxPhysicsLayers) {
                return;
            }

            layerNames[static_cast<size_t>(layerIndex)] = name.empty()
                ? ("Layer " + std::to_string(layerIndex))
                : name;
        }

        int PhysicsLayerSettings::GetLayerIndex(const std::string& name) const
        {
            const std::string normalized = ToLower(Trim(name));
            for (int i = 0; i < layerCount; ++i) {
                if (ToLower(layerNames[static_cast<size_t>(i)]) == normalized) {
                    return i;
                }
            }

            return 0;
        }

        bool PhysicsLayerSettings::GetLayerCollision(int layerA, int layerB) const
        {
            ClampLayerIndex(layerA);
            ClampLayerIndex(layerB);
            return collisionMatrix[layerA][layerB];
        }

        void PhysicsLayerSettings::SetLayerCollision(int layerA, int layerB, bool collides)
        {
            ClampLayerIndex(layerA);
            ClampLayerIndex(layerB);
            collisionMatrix[layerA][layerB] = collides;
            collisionMatrix[layerB][layerA] = collides;
        }

        int PhysicsLayerSettings::GetLayerGroup(int layerIndex) const
        {
            ClampLayerIndex(layerIndex);
            return 1 << layerIndex;
        }

        int PhysicsLayerSettings::GetLayerMask(int layerIndex) const
        {
            ClampLayerIndex(layerIndex);

            int mask = 0;
            for (int otherLayer = 0; otherLayer < layerCount; ++otherLayer) {
                if (collisionMatrix[layerIndex][otherLayer]) {
                    mask |= (1 << otherLayer);
                }
            }

            return mask;
        }

        void PhysicsLayerSettings::ApplyToCollisionObject(::btCollisionObject* object, int layerIndex) const
        {
            if (!object) {
                return;
            }

            const int group = GetLayerGroup(layerIndex);
            const int mask = GetLayerMask(layerIndex);

            if (btBroadphaseProxy* proxy = object->getBroadphaseHandle()) {
                proxy->m_collisionFilterGroup = group;
                proxy->m_collisionFilterMask = mask;
            }
        }

        void PhysicsLayerSettings::ApplyToGameObject(Scene::GameObject* gameObject) const
        {
            if (!gameObject) {
                return;
            }

            const int layerIndex = gameObject->GetCollisionLayer();

            auto applyFromCollider = [&](auto* colliderComponent) {
                if (!colliderComponent) {
                    return;
                }

                ::btCollisionObject* collisionObject = colliderComponent->GetBulletCollisionObject();
                if (collisionObject) {
                    ApplyToCollisionObject(collisionObject, layerIndex);
                }
            };

            applyFromCollider(gameObject->GetComponent<Scene::BoxColliderComponent>());
            applyFromCollider(gameObject->GetComponent<Scene::SphereColliderComponent>());
            applyFromCollider(gameObject->GetComponent<Scene::CapsuleColliderComponent>());

            if (Scene::RigidBodyComponent* rigidBodyComponent = gameObject->GetComponent<Scene::RigidBodyComponent>()) {
                if (RigidBody* rigidBody = rigidBodyComponent->GetRigidBody()) {
                    if (btRigidBody* bulletBody = rigidBody->GetBulletRigidBody()) {
                        ApplyToCollisionObject(bulletBody, layerIndex);
                    }
                }
            }
        }

        bool PhysicsLayerSettings::LoadFromFile(const std::filesystem::path& path)
        {
            std::ifstream file(path);
            if (!file.is_open()) {
                return false;
            }

            ResetToDefaults();

            std::string line;
            while (std::getline(file, line)) {
                const std::string trimmed = Trim(line);
                if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
                    continue;
                }

                const size_t separator = trimmed.find('=');
                if (separator == std::string::npos) {
                    continue;
                }

                const std::string key = Trim(trimmed.substr(0, separator));
                const std::string value = Trim(trimmed.substr(separator + 1));

                if (key == "LayerCount") {
                    layerCount = std::clamp(std::stoi(value), 1, kMaxPhysicsLayers);
                    continue;
                }

                if (key.rfind("Layer", 0) == 0 && key.size() > 5) {
                    try {
                        const int layerIndex = std::stoi(key.substr(5));
                        if (layerIndex >= 0 && layerIndex < kMaxPhysicsLayers) {
                            layerNames[static_cast<size_t>(layerIndex)] = value;
                        }
                    }
                    catch (...) {
                    }
                    continue;
                }

                const size_t matrixSeparator = key.find('_');
                if (matrixSeparator != std::string::npos && key.rfind("Matrix", 0) == 0) {
                    try {
                        const int row = std::stoi(key.substr(6, matrixSeparator - 6));
                        const int column = std::stoi(key.substr(matrixSeparator + 1));
                        if (row >= 0 && row < kMaxPhysicsLayers &&
                            column >= 0 && column < kMaxPhysicsLayers) {
                            const bool collides = (value == "1");
                            collisionMatrix[row][column] = collides;
                            collisionMatrix[column][row] = collides;
                        }
                    }
                    catch (...) {
                    }
                }
            }

            EnsureLayerCountInRange();
            RTB_INFO("PhysicsLayerSettings: Loaded " + path.string());
            return true;
        }

        bool PhysicsLayerSettings::SaveToFile(const std::filesystem::path& path) const
        {
            std::error_code ec;
            const std::filesystem::path parentPath = path.parent_path();
            if (!parentPath.empty()) {
                std::filesystem::create_directories(parentPath, ec);
            }

            std::ofstream file(path);
            if (!file.is_open()) {
                RTB_ERROR("PhysicsLayerSettings: Failed to write " + path.string());
                return false;
            }

            file << "# Project physics collision layers (editor). Engine default is only \"Default\".\n";
            file << "LayerCount=" << layerCount << "\n";
            for (int i = 0; i < layerCount; ++i) {
                file << "Layer" << i << "=" << layerNames[static_cast<size_t>(i)] << "\n";
            }

            file << "\n";
            for (int row = 0; row < layerCount; ++row) {
                for (int column = row; column < layerCount; ++column) {
                    file << "Matrix" << row << "_" << column << "="
                        << (collisionMatrix[row][column] ? "1" : "0") << "\n";
                }
            }

            return true;
        }

    }
}
