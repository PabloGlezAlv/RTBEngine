#include "SceneLoader.h"
#include "ComponentRegistry.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/Component.h"
#include "../ECS/MeshRenderer.h"
#include "../ECS/LightComponent.h"
#include "../ECS/AudioSourceComponent.h"
#include "../ECS/RigidBodyComponent.h"
#include "../ECS/BoxColliderComponent.h"
#include "../Core/ResourceManager.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "../Rendering/Lighting/PointLight.h"
#include "../Rendering/Lighting/SpotLight.h"
#include "../Rendering/ModelLoader.h"
#include "../Physics/RigidBody.h"
#include "../Physics/BoxCollider.h"
#include "../Math/Math.h"
#include "../UI/Canvas.h"
#include "../UI/Elements/UIText.h"
#include "../UI/Elements/UIImage.h"
#include "../UI/Elements/UIPanel.h"
#include "../UI/Elements/UIButton.h"
#include "../ECS/CameraComponent.h"
#include "../ECS/FreeLookCamera.h"
#include "../Animation/Animator.h"

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>
#include <cstdio>

namespace RTBEngine {
    namespace Scripting {

        #pragma region Lua Helpers
        // Helper wrapper for Quaternion::FromEulerAngles (converts degrees to radians)
        static Math::Quaternion QuaternionFromEulerAngles(float pitch, float yaw, float roll) {
            const float toRadians = 3.14159265f / 180.0f;
            return Math::Quaternion::FromEulerAngles(
                pitch * toRadians,
                yaw * toRadians,
                roll * toRadians
            );
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

        static int ReadOptionalInt(lua_State* L, int tableIndex, const char* fieldName, int defaultValue) {
            lua_getfield(L, tableIndex, fieldName);
            int result = defaultValue;
            if (lua_isnumber(L, -1)) {
                result = static_cast<int>(lua_tonumber(L, -1));
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

        static Math::Vector2 ReadOptionalVector2(lua_State* L, int tableIndex, const char* fieldName, const Math::Vector2& defaultValue) {
            lua_getfield(L, tableIndex, fieldName);
            Math::Vector2 result = defaultValue;
            if (lua_isuserdata(L, -1)) {
                auto vecResult = luabridge::Stack<Math::Vector2>::get(L, -1);
                if (vecResult) {
                    result = vecResult.value();
                }
            }
            lua_pop(L, 1);
            return result;
        }

        static Math::Vector4 ReadOptionalVector4(lua_State* L, int tableIndex, const char* fieldName, const Math::Vector4& defaultValue) {
            lua_getfield(L, tableIndex, fieldName);
            Math::Vector4 result = defaultValue;
            if (lua_isuserdata(L, -1)) {
                auto vecResult = luabridge::Stack<Math::Vector4>::get(L, -1);
                if (vecResult) {
                    result = vecResult.value();
                }
            }
            lua_pop(L, 1);
            return result;
        }

        #pragma endregion

        #pragma region Component Configurators

        static void ConfigureRectTransform(lua_State* L, int tableIndex, UI::RectTransform* rect) {
            if (!rect) return;

            // AnchorMin (Vector2)
            rect->SetAnchorMin(ReadOptionalVector2(L, tableIndex, "anchorMin", rect->GetAnchorMin()));

            // AnchorMax (Vector2)
            rect->SetAnchorMax(ReadOptionalVector2(L, tableIndex, "anchorMax", rect->GetAnchorMax()));

            // Pivot (Vector2)
            rect->SetPivot(ReadOptionalVector2(L, tableIndex, "pivot", rect->GetPivot()));

            // AnchoredPosition (Vector2)
            rect->SetAnchoredPosition(ReadOptionalVector2(L, tableIndex, "anchoredPosition", rect->GetAnchoredPosition()));

            // Size (Vector2)
            rect->SetSize(ReadOptionalVector2(L, tableIndex, "sizeDelta", rect->GetSize()));
        }

        static void ConfigureCanvas(lua_State* L, int tableIndex, UI::Canvas* comp) {
            comp->SetSortOrder(static_cast<int>(ReadOptionalFloat(L, tableIndex, "sortOrder", 0.0f)));
        }

        static void ConfigureUIText(lua_State* L, int tableIndex, UI::UIText* comp) {
            comp->SetText(ReadOptionalString(L, tableIndex, "text", "New Text"));
            comp->SetColor(ReadOptionalVector4(L, tableIndex, "color", Math::Vector4(0, 0, 0, 1)));
            comp->SetFontSize(ReadOptionalFloat(L, tableIndex, "fontSize", 14.0f));
            
            // Alignment mapping 0: Left, 1: Center, 2: Right
            int align = static_cast<int>(ReadOptionalFloat(L, tableIndex, "alignment", 0.0f));
            comp->SetAlignment(static_cast<UI::TextAlignment>(align));

            ConfigureRectTransform(L, tableIndex, comp->GetRectTransform());
        }

        static void ConfigureUIImage(lua_State* L, int tableIndex, UI::UIImage* comp) {
            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();
            std::string texturePath = ReadOptionalString(L, tableIndex, "texture", "");
            if (!texturePath.empty()) {
                Rendering::Texture* tex = resources.LoadTexture(texturePath);
                if (tex) comp->SetTexture(tex);
            }

            comp->SetTint(ReadOptionalVector4(L, tableIndex, "color", Math::Vector4(1, 1, 1, 1)));
            comp->SetPreserveAspect(ReadOptionalBool(L, tableIndex, "preserveAspect", false));

            ConfigureRectTransform(L, tableIndex, comp->GetRectTransform());
        }

        static void ConfigureUIPanel(lua_State* L, int tableIndex, UI::UIPanel* comp) {
            comp->SetBackgroundColor(ReadOptionalVector4(L, tableIndex, "color", Math::Vector4(1, 1, 1, 1)));
            comp->SetBorderColor(ReadOptionalVector4(L, tableIndex, "borderColor", Math::Vector4(1, 1, 1, 1)));
            comp->SetBorderThickness(ReadOptionalFloat(L, tableIndex, "borderThickness", 0.0f));
            comp->SetHasBorder(ReadOptionalBool(L, tableIndex, "hasBorder", false));

            ConfigureRectTransform(L, tableIndex, comp->GetRectTransform());
        }

        static void ConfigureUIButton(lua_State* L, int tableIndex, UI::UIButton* comp) {
            comp->SetNormalColor(ReadOptionalVector4(L, tableIndex, "normalColor", Math::Vector4(1, 1, 1, 1)));
            comp->SetHoveredColor(ReadOptionalVector4(L, tableIndex, "hoveredColor", Math::Vector4(0.9f, 0.9f, 0.9f, 1)));
            comp->SetPressedColor(ReadOptionalVector4(L, tableIndex, "pressedColor", Math::Vector4(0.7f, 0.7f, 0.7f, 1)));
            comp->SetDisabledColor(ReadOptionalVector4(L, tableIndex, "disabledColor", Math::Vector4(0.5f, 0.5f, 0.5f, 1)));
        }

        static void ConfigureMeshRenderer(lua_State* L, int tableIndex, ECS::MeshRenderer* comp) {
            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();

            // shader (string name, default "basic") - load first so materials can use it
            std::string shaderName = ReadOptionalString(L, tableIndex, "shader", "basic");
            Rendering::Shader* shader = resources.GetShader(shaderName);
            if (shader) {
                comp->SetShader(shader);
            }

            // model (string path) - loads model with embedded materials/textures
            std::string modelPath = ReadOptionalString(L, tableIndex, "model", "");
            if (!modelPath.empty()) {
                Rendering::ModelData modelData = Rendering::ModelLoader::LoadModelWithAnimations(modelPath);

                if (!modelData.meshes.empty()) {
                    comp->SetMeshes(modelData.meshes);

                    // Apply embedded materials if available
                    if (!modelData.materials.empty() && shader) {
                        // Create textures from embedded data
                        std::vector<Rendering::Texture*> embeddedTextures;
                        for (size_t i = 0; i < modelData.embeddedTextures.size(); i++) {
                            const Rendering::EmbeddedTexture& embTex = modelData.embeddedTextures[i];
                            Rendering::Texture* tex = new Rendering::Texture();
                            bool loaded = false;

                            if (embTex.isCompressed) {
                                loaded = tex->LoadFromCompressedMemory(embTex.data.data(), static_cast<int>(embTex.data.size()));
                            } else {
                                loaded = tex->LoadFromMemory(embTex.data.data(), embTex.width, embTex.height, embTex.channels);
                            }

                            if (loaded) {
                                embeddedTextures.push_back(tex);
                            } else {
                                embeddedTextures.push_back(nullptr);
                                delete tex;
                            }
                        }

                        // Create materials for each mesh
                        std::vector<Rendering::Material*> meshMats;
                        for (Rendering::Mesh* mesh : modelData.meshes) {
                            int matIdx = mesh->GetMaterialIndex();
                            if (matIdx >= 0 && matIdx < static_cast<int>(modelData.materials.size())) {
                                const Rendering::LoadedMaterial& loadedMat = modelData.materials[matIdx];

                                Rendering::Material* mat = new Rendering::Material(shader);
                                mat->SetDiffuseColor(loadedMat.diffuseColor);

                                // Apply embedded texture if available
                                if (loadedMat.embeddedTextureIndex >= 0 &&
                                    loadedMat.embeddedTextureIndex < static_cast<int>(embeddedTextures.size()) &&
                                    embeddedTextures[loadedMat.embeddedTextureIndex]) {
                                    mat->SetTexture(embeddedTextures[loadedMat.embeddedTextureIndex]);
                                }
                                else if (!loadedMat.diffuseTexturePath.empty()) {
                                    Rendering::Texture* tex = resources.LoadTexture(loadedMat.diffuseTexturePath);
                                    if (tex) {
                                        mat->SetTexture(tex);
                                    }
                                }

                                meshMats.push_back(mat);
                            } else {
                                meshMats.push_back(nullptr);
                            }
                        }
                        comp->SetMeshMaterials(meshMats);
                    }
                }
            }
            else {
                // mesh (string path) - simple mesh loading without materials
                std::string meshPath = ReadOptionalString(L, tableIndex, "mesh", "");
                if (!meshPath.empty()) {
                    const std::vector<Rendering::Mesh*>& meshes = resources.LoadModelMeshes(meshPath);
                    if (!meshes.empty()) {
                        comp->SetMeshes(meshes);
                    }
                }
            }

            // texture (string path) - override texture for all meshes
            std::string texturePath = ReadOptionalString(L, tableIndex, "texture", "");
            if (!texturePath.empty()) {
                Rendering::Texture* texture = resources.LoadTexture(texturePath);
                if (texture) {
                    comp->SetTexture(texture);
                }
            }
        }

        static void ConfigureLightComponent(lua_State* L, int tableIndex, ECS::LightComponent* comp) {
            std::string lightType = ReadOptionalString(L, tableIndex, "lightType", "Directional");

            Math::Vector3 color = ReadOptionalVector3(L, tableIndex, "color", Math::Vector3(1.0f, 1.0f, 1.0f));
            float intensity = ReadOptionalFloat(L, tableIndex, "intensity", 1.0f);

            if (lightType == "Directional") {
                auto dirLight = std::make_unique<Rendering::DirectionalLight>();
                dirLight->SetColor(color);
                dirLight->SetIntensity(intensity);

                bool castShadows = ReadOptionalBool(L, tableIndex, "castShadows", false);
                if (castShadows) {
                    dirLight->SetCastShadows(true);

                    int shadowMapResolution = ReadOptionalInt(L, tableIndex, "shadowMapResolution", 1024);
                    if (shadowMapResolution != 1024) {
                        dirLight->SetShadowMapResolution(shadowMapResolution);
                    }

                    float shadowBias = ReadOptionalFloat(L, tableIndex, "shadowBias", 0.005f);
                    dirLight->SetShadowBias(shadowBias);
                }

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
        }

        static void ConfigureBoxCollider(lua_State* L, int tableIndex, ECS::BoxColliderComponent* comp, ECS::GameObject* gameObject) {
            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();

            // Size - either from mesh or explicit
            std::string colliderMesh = ReadOptionalString(L, tableIndex, "mesh", "");
            if (!colliderMesh.empty()) {
                Rendering::Mesh* mesh = resources.LoadModel(colliderMesh);
                if (mesh) {
                    // Create temporary BoxCollider to calculate size from mesh
                    Physics::BoxCollider tempCollider(mesh);
                    Math::Vector3 size = tempCollider.GetSize() * gameObject->GetTransform().GetScale();
                    comp->SetSize(size);
                }
            }
            else {
                // Use explicit size or default
                Math::Vector3 size = ReadOptionalVector3(L, tableIndex, "size", Math::Vector3(1.0f, 1.0f, 1.0f));
                comp->SetSize(size * gameObject->GetTransform().GetScale());
            }

            // Trigger
            comp->SetIsTrigger(ReadOptionalBool(L, tableIndex, "isTrigger", false));
        }

        static void ConfigureCameraComponent(lua_State* L, int tableIndex, ECS::CameraComponent* comp) {
            comp->SetFOV(ReadOptionalFloat(L, tableIndex, "fov", 45.0f));
            comp->SetNearPlane(ReadOptionalFloat(L, tableIndex, "nearPlane", 0.1f));
            comp->SetFarPlane(ReadOptionalFloat(L, tableIndex, "farPlane", 100.0f));
            comp->SetAsMain(ReadOptionalBool(L, tableIndex, "isMain", false));

            std::string projType = ReadOptionalString(L, tableIndex, "projection", "Perspective");
            if (projType == "Orthographic") {
                comp->SetProjectionType(Rendering::ProjectionType::Orthographic);
                comp->SetOrthographicSize(ReadOptionalFloat(L, tableIndex, "orthoSize", 5.0f));
            }
        }

        static void ConfigureFreeLookCamera(lua_State* L, int tableIndex, ECS::FreeLookCamera* comp) {
            comp->SetMoveSpeed(ReadOptionalFloat(L, tableIndex, "moveSpeed", 5.0f));
            comp->SetLookSpeed(ReadOptionalFloat(L, tableIndex, "lookSpeed", 0.1f));
        }

        static void ConfigureAnimator(lua_State* L, int tableIndex, Animation::Animator* comp) {
            // model (string path) - Load model with animations
            std::string modelPath = ReadOptionalString(L, tableIndex, "model", "");
            if (!modelPath.empty()) {
                Rendering::ModelData modelData = Rendering::ModelLoader::LoadModelWithAnimations(modelPath);

                if (modelData.skeleton) {
                    comp->SetSkeleton(modelData.skeleton);
                }

                // Store meshes with bone data
                if (!modelData.meshes.empty()) {
                    comp->SetMeshes(modelData.meshes);

                    // Update MeshRenderer with all meshes from the model
                    ECS::GameObject* owner = comp->GetOwner();
                    if (owner) {
                        ECS::MeshRenderer* meshRenderer = owner->GetComponent<ECS::MeshRenderer>();
                        if (meshRenderer) {
                            meshRenderer->SetMeshes(modelData.meshes);

                            // Apply materials from model if available
                            if (!modelData.materials.empty()) {
                                Rendering::Shader* shader = meshRenderer->GetMaterial()->GetShader();

                                // Create textures from embedded data if available
                                std::vector<Rendering::Texture*> embeddedTextures;
                                for (size_t i = 0; i < modelData.embeddedTextures.size(); i++) {
                                    const Rendering::EmbeddedTexture& embTex = modelData.embeddedTextures[i];
                                    Rendering::Texture* tex = new Rendering::Texture();
                                    bool loaded = false;

                                    if (embTex.isCompressed) {
                                        loaded = tex->LoadFromCompressedMemory(embTex.data.data(), static_cast<int>(embTex.data.size()));
                                    } else {
                                        loaded = tex->LoadFromMemory(embTex.data.data(), embTex.width, embTex.height, embTex.channels);
                                    }

                                    if (loaded) {
                                        embeddedTextures.push_back(tex);
                                    } else {
                                        embeddedTextures.push_back(nullptr);
                                        delete tex;
                                    }
                                }

                                // Create materials for each mesh based on materialIndex
                                std::vector<Rendering::Material*> meshMats;
                                for (Rendering::Mesh* mesh : modelData.meshes) {
                                    int matIdx = mesh->GetMaterialIndex();
                                    if (matIdx >= 0 && matIdx < static_cast<int>(modelData.materials.size())) {
                                        const Rendering::LoadedMaterial& loadedMat = modelData.materials[matIdx];

                                        // Create new material
                                        Rendering::Material* mat = new Rendering::Material(shader);
                                        mat->SetDiffuseColor(loadedMat.diffuseColor);

                                        // Try embedded texture first, then external file
                                        if (loadedMat.embeddedTextureIndex >= 0 &&
                                            loadedMat.embeddedTextureIndex < static_cast<int>(embeddedTextures.size()) &&
                                            embeddedTextures[loadedMat.embeddedTextureIndex]) {
                                            mat->SetTexture(embeddedTextures[loadedMat.embeddedTextureIndex]);
                                        }
                                        else if (!loadedMat.diffuseTexturePath.empty()) {
                                            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();
                                            Rendering::Texture* tex = resources.LoadTexture(loadedMat.diffuseTexturePath);
                                            if (tex) {
                                                mat->SetTexture(tex);
                                            }
                                        }

                                        meshMats.push_back(mat);
                                    } else {
                                        meshMats.push_back(nullptr);  // Use default material
                                    }
                                }
                                meshRenderer->SetMeshMaterials(meshMats);
                            }
                        }
                    }
                }

                // Add all animations from the model
                for (const auto& clip : modelData.animations) {
                    comp->AddClip(clip->GetName(), clip);
                }
            }

            // defaultClip (string) - Name of animation to play by default
            std::string defaultClip = ReadOptionalString(L, tableIndex, "defaultClip", "");
            if (!defaultClip.empty()) {
                bool loop = ReadOptionalBool(L, tableIndex, "loop", true);
                comp->Play(defaultClip, loop);
            }

            // speed (float) - Playback speed
            comp->SetSpeed(ReadOptionalFloat(L, tableIndex, "speed", 1.0f));
        }


        #pragma endregion

        #pragma region SceneLoader Implementation
        void SceneLoader::SetupLuaBindings(lua_State* L) {
            // Bind Math::Vector3
            luabridge::getGlobalNamespace(L)
                .beginClass<Math::Vector3>("Vector3")
                .addConstructor<void(*)(float, float, float)>()
                .addProperty("x", &Math::Vector3::x)
                .addProperty("y", &Math::Vector3::y)
                .addProperty("z", &Math::Vector3::z)
                .endClass();

            // Bind Math::Vector2
            luabridge::getGlobalNamespace(L)
                .beginClass<Math::Vector2>("Vector2")
                .addConstructor<void(*)(float, float)>()
                .addProperty("x", &Math::Vector2::x)
                .addProperty("y", &Math::Vector2::y)
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
                std::vector<std::pair<ECS::GameObject*, std::string>> parentingRequests;

                for (int i = 1; i <= gameObjectsCount; i++) {
                    lua_geti(L, -1, i);  // Push gameObjects[i]

                    if (lua_istable(L, -1)) {
                        ECS::GameObject* go = ProcessGameObject(L, lua_gettop(L), scene, parentingRequests);
                        if (go) {
                            scene->AddGameObject(go);
                        }
                    }

                    lua_pop(L, 1);  // Pop gameObjects[i]
                }

                // Process parenting requests
                for (const auto& req : parentingRequests) {
                    ECS::GameObject* child = req.first;
                    const std::string& parentName = req.second;
                    ECS::GameObject* parent = scene->FindGameObject(parentName);
                    
                    if (parent) {
                        child->SetParent(parent);
                    } else {
                        printf("SceneLoader: Warning - Parent '%s' not found for object '%s'\n", parentName.c_str(), child->GetName().c_str());
                    }
                }
            }
            lua_pop(L, 1);  // Pop gameObjects array

            lua_close(L);
            return scene;
        }

        ECS::GameObject* SceneLoader::ProcessGameObject(lua_State* L, int tableIndex, ECS::Scene* scene, std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests) {
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

            // Read parent name
            std::string parentName = ReadOptionalString(L, tableIndex, "parent", "");
            if (!parentName.empty()) {
                parentingRequests.push_back({ go, parentName });
            }

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
                            else if (componentType == "BoxColliderComponent") {
                                ConfigureBoxCollider(L, componentTableIndex, static_cast<ECS::BoxColliderComponent*>(comp), gameObject);
                            }
                            else if (componentType == "Canvas") {
                                ConfigureCanvas(L, componentTableIndex, static_cast<UI::Canvas*>(comp));
                            }
                            else if (componentType == "UIText") {
                                ConfigureUIText(L, componentTableIndex, static_cast<UI::UIText*>(comp));
                            }
                            else if (componentType == "UIImage") {
                                ConfigureUIImage(L, componentTableIndex, static_cast<UI::UIImage*>(comp));
                            }
                            else if (componentType == "UIPanel") {
                                ConfigureUIPanel(L, componentTableIndex, static_cast<UI::UIPanel*>(comp));
                            }
                            else if (componentType == "UIButton") {
                                ConfigureUIButton(L, componentTableIndex, static_cast<UI::UIButton*>(comp));
                            }
                            else if (componentType == "CameraComponent") {
                                ConfigureCameraComponent(L, componentTableIndex, static_cast<ECS::CameraComponent*>(comp));
                            }
                            else if (componentType == "FreeLookCamera") {
                                ConfigureFreeLookCamera(L, componentTableIndex, static_cast<ECS::FreeLookCamera*>(comp));
                            }
                            else if (componentType == "Animator") {
                                ConfigureAnimator(L, componentTableIndex, static_cast<Animation::Animator*>(comp));
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
        #pragma endregion

    }
}
