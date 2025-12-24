#include "SceneLoader.h"
#include "ComponentRegistry.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/Component.h"
#include "../ECS/MeshRenderer.h"
#include "../ECS/LightComponent.h"
#include "../ECS/AudioSourceComponent.h"
#include "../ECS/RigidBodyComponent.h"
#include "../Core/ResourceManager.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "../Rendering/Lighting/PointLight.h"
#include "../Rendering/Lighting/SpotLight.h"
#include "../Physics/RigidBody.h"
#include "../Physics/BoxCollider.h"
#include "../Math/Math.h"

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>
#include <cstdio>

namespace RTBEngine {
    namespace Scripting {

        // Helper wrapper for Quaternion::FromEulerAngles (needed for LuaBridge)
        static Math::Quaternion QuaternionFromEulerAngles(float pitch, float yaw, float roll) {
            return Math::Quaternion::FromEulerAngles(pitch, yaw, roll);
        }

        static std::string ReadOptionalString(lua_State* L, int tableIndex, const char* fieldName, const std::string& defaultValue = "") {
            lua_getfield(L, tableIndex, fieldName);
            std::string result = defaultValue;
            if (lua_isstring(L, -1)) {
                result = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
            return result;
        }

        static float ReadOptionalFloat(lua_State* L, int tableIndex, const char* fieldName, float defaultValue) {
            lua_getfield(L, tableIndex, fieldName);
            float result = defaultValue;
            if (lua_isnumber(L, -1)) {
                result = static_cast<float>(lua_tonumber(L, -1));
            }
            lua_pop(L, 1);
            return result;
        }

        static bool ReadOptionalBool(lua_State* L, int tableIndex, const char* fieldName, bool defaultValue) {
            lua_getfield(L, tableIndex, fieldName);
            bool result = defaultValue;
            if (lua_isboolean(L, -1)) {
                result = lua_toboolean(L, -1) != 0;
            }
            lua_pop(L, 1);
            return result;
        }

        static Math::Vector3 ReadOptionalVector3(lua_State* L, int tableIndex, const char* fieldName, const Math::Vector3& defaultValue) {
            lua_getfield(L, tableIndex, fieldName);
            Math::Vector3 result = defaultValue;
            if (lua_isuserdata(L, -1)) {
                auto vecResult = luabridge::Stack<Math::Vector3>::get(L, -1);
                if (vecResult) {
                    result = vecResult.value();
                }
            }
            lua_pop(L, 1);
            return result;
        }

        // ============================================================
        // Component configurators
        // ============================================================

        static void ConfigureMeshRenderer(lua_State* L, int tableIndex, ECS::MeshRenderer* comp) {
            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();

            // mesh (string path)
            std::string meshPath = ReadOptionalString(L, tableIndex, "mesh", "");
            if (!meshPath.empty()) {
                Rendering::Mesh* mesh = resources.LoadModel(meshPath);
                if (mesh) {
                    comp->SetMesh(mesh);
                }
            }

            // shader (string name, default "basic")
            std::string shaderName = ReadOptionalString(L, tableIndex, "shader", "basic");
            Rendering::Shader* shader = resources.GetShader(shaderName);
            if (shader) {
                comp->SetShader(shader);
            }

            // texture (string path)
            std::string texturePath = ReadOptionalString(L, tableIndex, "texture", "");
            if (!texturePath.empty()) {
                Rendering::Texture* texture = resources.LoadTexture(texturePath);
                if (texture) {
                    comp->SetTexture(texture);
                }
            }
        }

        static void ConfigureLightComponent(lua_State* L, int tableIndex, ECS::LightComponent* comp) {
            // lightType (string: "Directional", "Point", "Spot")
            std::string lightType = ReadOptionalString(L, tableIndex, "lightType", "Directional");

            // Common properties
            Math::Vector3 color = ReadOptionalVector3(L, tableIndex, "color", Math::Vector3(1.0f, 1.0f, 1.0f));
            float intensity = ReadOptionalFloat(L, tableIndex, "intensity", 1.0f);

            if (lightType == "Directional") {
                auto dirLight = std::make_unique<Rendering::DirectionalLight>();
                dirLight->SetColor(color);
                dirLight->SetIntensity(intensity);
                comp->SetLight(std::move(dirLight));
            }
            else if (lightType == "Point") {
                auto pointLight = std::make_unique<Rendering::PointLight>();
                pointLight->SetColor(color);
                pointLight->SetIntensity(intensity);
                pointLight->SetRange(ReadOptionalFloat(L, tableIndex, "range", 50.0f));
                comp->SetLight(std::move(pointLight));
            }
            else if (lightType == "Spot") {
                auto spotLight = std::make_unique<Rendering::SpotLight>();
                spotLight->SetColor(color);
                spotLight->SetIntensity(intensity);
                spotLight->SetRange(ReadOptionalFloat(L, tableIndex, "range", 50.0f));
                float innerCutOff = ReadOptionalFloat(L, tableIndex, "innerCutOff", 12.5f);
                float outerCutOff = ReadOptionalFloat(L, tableIndex, "outerCutOff", 15.0f);
                spotLight->SetCutOff(innerCutOff, outerCutOff);
                comp->SetLight(std::move(spotLight));
            }
        }

        static void ConfigureAudioSource(lua_State* L, int tableIndex, ECS::AudioSourceComponent* comp) {
            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();

            // clip (string path)
            std::string clipPath = ReadOptionalString(L, tableIndex, "clip", "");
            if (!clipPath.empty()) {
                Audio::AudioClip* clip = resources.LoadAudioClip(clipPath);
                if (clip) {
                    comp->SetClip(clip);
                }
            }

            // volume, pitch, loop, playOnStart
            comp->SetVolume(ReadOptionalFloat(L, tableIndex, "volume", 1.0f));
            comp->SetPitch(ReadOptionalFloat(L, tableIndex, "pitch", 1.0f));
            comp->SetLoop(ReadOptionalBool(L, tableIndex, "loop", false));
            comp->SetPlayOnStart(ReadOptionalBool(L, tableIndex, "playOnStart", false));
        }

        static void ConfigureRigidBody(lua_State* L, int tableIndex, ECS::RigidBodyComponent* comp, ECS::GameObject* gameObject) {
            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();

            // Create RigidBody
            auto rigidBody = std::make_unique<Physics::RigidBody>();

            // bodyType (string: "Static", "Dynamic", "Kinematic")
            std::string bodyType = ReadOptionalString(L, tableIndex, "bodyType", "Dynamic");
            if (bodyType == "Static") {
                rigidBody->SetType(Physics::RigidBodyType::Static);
            }
            else if (bodyType == "Dynamic") {
                rigidBody->SetType(Physics::RigidBodyType::Dynamic);
            }
            else if (bodyType == "Kinematic") {
                rigidBody->SetType(Physics::RigidBodyType::Kinematic);
            }

            rigidBody->SetMass(ReadOptionalFloat(L, tableIndex, "mass", 1.0f));
            rigidBody->SetFriction(ReadOptionalFloat(L, tableIndex, "friction", 0.5f));
            rigidBody->SetRestitution(ReadOptionalFloat(L, tableIndex, "restitution", 0.0f));

            comp->SetRigidBody(std::move(rigidBody));

            // Collider - for now only Box, uses mesh if specified
            std::string colliderMesh = ReadOptionalString(L, tableIndex, "colliderMesh", "");
            if (!colliderMesh.empty()) {
                Rendering::Mesh* mesh = resources.LoadModel(colliderMesh);
                if (mesh) {
                    auto boxCollider = std::make_unique<Physics::BoxCollider>(mesh);
                    // Scale collider by GameObject scale
                    boxCollider->SetSize(boxCollider->GetSize() * gameObject->GetTransform().GetScale());
                    comp->SetCollider(std::move(boxCollider));
                }
            }
        }

        void SceneLoader::SetupLuaBindings(lua_State* L) {
            // Bind Math::Vector3
            luabridge::getGlobalNamespace(L)
                .beginClass<Math::Vector3>("Vector3")
                .addConstructor<void(*)(float, float, float)>()
                .addProperty("x", &Math::Vector3::x)
                .addProperty("y", &Math::Vector3::y)
                .addProperty("z", &Math::Vector3::z)
                .endClass();

            // Bind Math::Quaternion (only FromEulerAngles for now)
            luabridge::getGlobalNamespace(L)
                .beginClass<Math::Quaternion>("Quaternion")
                .addStaticFunction("FromEulerAngles", QuaternionFromEulerAngles)
                .endClass();

            // Bind Math::Vector4 (for colors)
            luabridge::getGlobalNamespace(L)
                .beginClass<Math::Vector4>("Vector4")
                .addConstructor<void(*)(float, float, float, float)>()
                .addProperty("x", &Math::Vector4::x)
                .addProperty("y", &Math::Vector4::y)
                .addProperty("z", &Math::Vector4::z)
                .addProperty("w", &Math::Vector4::w)
                .endClass();
        }

        ECS::Scene* SceneLoader::LoadScene(const std::string& filePath) {
            // Create Lua state
            lua_State* L = luaL_newstate();
            luaL_openlibs(L);

            // Setup bindings
            SetupLuaBindings(L);

            // Load and execute Lua file
            if (luaL_dofile(L, filePath.c_str()) != LUA_OK) {
                printf("SceneLoader: Failed to load file '%s': %s\n", filePath.c_str(), lua_tostring(L, -1));
                lua_close(L);
                return nullptr;
            }

            // Get CreateScene function
            lua_getglobal(L, "CreateScene");
            if (!lua_isfunction(L, -1)) {
                printf("SceneLoader: 'CreateScene' function not found in %s\n", filePath.c_str());
                lua_close(L);
                return nullptr;
            }

            // Call CreateScene()
            if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
                printf("SceneLoader: Error calling CreateScene(): %s\n", lua_tostring(L, -1));
                lua_close(L);
                return nullptr;
            }

            // Get scene table
            if (!lua_istable(L, -1)) {
                printf("SceneLoader: CreateScene() did not return a table\n");
                lua_close(L);
                return nullptr;
            }

            // Read scene name
            lua_getfield(L, -1, "name");
            std::string sceneName = "Unnamed Scene";
            if (lua_isstring(L, -1)) {
                sceneName = lua_tostring(L, -1);
            }
            lua_pop(L, 1);

            // Create scene
            ECS::Scene* scene = new ECS::Scene(sceneName);

            // Get gameObjects array
            lua_getfield(L, -1, "gameObjects");
            if (lua_istable(L, -1)) {
                int gameObjectsCount = luaL_len(L, -1);

                for (int i = 1; i <= gameObjectsCount; i++) {
                    lua_geti(L, -1, i);  // Push gameObjects[i]

                    if (lua_istable(L, -1)) {
                        ECS::GameObject* go = ProcessGameObject(L, lua_gettop(L), scene);
                        if (go) {
                            scene->AddGameObject(go);
                        }
                    }

                    lua_pop(L, 1);  // Pop gameObjects[i]
                }
            }
            lua_pop(L, 1);  // Pop gameObjects array

            lua_close(L);
            return scene;
        }

        ECS::GameObject* SceneLoader::ProcessGameObject(lua_State* L, int tableIndex, ECS::Scene* scene) {
            // Read name
            lua_getfield(L, tableIndex, "name");
            std::string name = "Unnamed";
            if (lua_isstring(L, -1)) {
                name = lua_tostring(L, -1);
            }
            lua_pop(L, 1);

            // Create GameObject
            ECS::GameObject* go = new ECS::GameObject(name);

            // Read position
            lua_getfield(L, tableIndex, "position");
            if (lua_isuserdata(L, -1)) {
                auto posResult = luabridge::Stack<Math::Vector3>::get(L, -1);
                if (posResult) {
                    go->GetTransform().SetPosition(posResult.value());
                }
            }
            lua_pop(L, 1);

            // Read rotation
            lua_getfield(L, tableIndex, "rotation");
            if (lua_isuserdata(L, -1)) {
                auto rotResult = luabridge::Stack<Math::Quaternion>::get(L, -1);
                if (rotResult) {
                    go->GetTransform().SetRotation(rotResult.value());
                }
            }
            lua_pop(L, 1);

            // Read scale
            lua_getfield(L, tableIndex, "scale");
            if (lua_isuserdata(L, -1)) {
                auto scaleResult = luabridge::Stack<Math::Vector3>::get(L, -1);
                if (scaleResult) {
                    go->GetTransform().SetScale(scaleResult.value());
                }
            }
            lua_pop(L, 1);

            // Process components
            lua_getfield(L, tableIndex, "components");
            if (lua_istable(L, -1)) {
                ProcessComponents(L, lua_gettop(L), go);
            }
            lua_pop(L, 1);

            return go;
        }

        void SceneLoader::ProcessComponents(lua_State* L, int arrayIndex, ECS::GameObject* gameObject) {
            int componentCount = luaL_len(L, arrayIndex);

            for (int i = 1; i <= componentCount; i++) {
                lua_geti(L, arrayIndex, i);  // Push components[i]
                int componentTableIndex = lua_gettop(L);

                if (lua_istable(L, -1)) {
                    // Read component type
                    lua_getfield(L, -1, "type");
                    if (lua_isstring(L, -1)) {
                        std::string componentType = lua_tostring(L, -1);
                        lua_pop(L, 1);  // Pop type early so we can read other fields

                        // Create component using registry
                        ECS::Component* comp = ComponentRegistry::GetInstance().CreateComponent(componentType);
                        if (comp) {
                            gameObject->AddComponent(comp);

                            // Configure component based on type
                            if (componentType == "MeshRenderer") {
                                ConfigureMeshRenderer(L, componentTableIndex, static_cast<ECS::MeshRenderer*>(comp));
                            }
                            else if (componentType == "LightComponent") {
                                ConfigureLightComponent(L, componentTableIndex, static_cast<ECS::LightComponent*>(comp));
                            }
                            else if (componentType == "AudioSourceComponent") {
                                ConfigureAudioSource(L, componentTableIndex, static_cast<ECS::AudioSourceComponent*>(comp));
                            }
                            else if (componentType == "RigidBodyComponent") {
                                ConfigureRigidBody(L, componentTableIndex, static_cast<ECS::RigidBodyComponent*>(comp), gameObject);
                            }
                            // Other component types use default values
                        }
                        else {
                            printf("SceneLoader: Failed to create component '%s'\n", componentType.c_str());
                        }
                    }
                    else {
                        lua_pop(L, 1);  // Pop type if not string
                    }
                }

                lua_pop(L, 1);  // Pop components[i]
            }
        }

    }
}
