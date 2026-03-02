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
#include "../UI/Elements/UIContainer.h"
#include "../ECS/CameraComponent.h"
#include "../ECS/FreeLookCamera.h"
#include "../Animation/Animator.h"
#include "../Reflection/TypeInfo.h"

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>
#include <cstdio>
#include "../RTBEngine.h"

#include "SceneLuaBindings.h"
#include "SceneComponentConfigurator.h"
#include "SceneParsingUtils.h"

namespace RTBEngine {
    namespace Scripting {
        #pragma region SceneLoader Implementation
        void SceneLoader::SetupLuaBindings(lua_State* L) {
            SceneLuaBindings::SetupLuaBindings(L);
        }

        ECS::Scene* SceneLoader::LoadScene(const std::string& filePath) {
            // Create Lua state
            lua_State* L = luaL_newstate();
            luaL_openlibs(L);

            // Setup bindings
            SetupLuaBindings(L);

            // Load and execute Lua file
            if (luaL_dofile(L, filePath.c_str()) != LUA_OK) {
                RTB_ERROR("SceneLoader: Failed to load file '" + filePath + "': " + std::string(lua_tostring(L, -1)));
                lua_close(L);
                return nullptr;
            }

            // Get CreateScene function
            lua_getglobal(L, "CreateScene");
            if (!lua_isfunction(L, -1)) {
                RTB_ERROR("SceneLoader: 'CreateScene' function not found in " + filePath);
                lua_close(L);
                return nullptr;
            }

            // Call CreateScene()
            if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
                RTB_ERROR("SceneLoader: Error calling CreateScene(): " + std::string(lua_tostring(L, -1)));
                lua_close(L);
                return nullptr;
            }

            // Validate root scene table before using it
            if (!SceneParsingUtils::ValidateSceneTable(L, -1, filePath)) {
                lua_close(L);
                return nullptr;
            }

            const std::string sceneName = SceneParsingUtils::ReadOptionalString(L, -1, "name", "Unnamed Scene");

            // Create scene
            ECS::Scene* scene = new ECS::Scene(sceneName);

            // Read skybox settings
            std::string skyboxPath = SceneParsingUtils::ReadOptionalString(L, -1, "skybox", "");
            bool skyboxEnabled = SceneParsingUtils::ReadOptionalBool(L, -1, "skyboxEnabled", true);

            scene->SetSkyboxEnabled(skyboxEnabled);
            if (!skyboxPath.empty()) {
                Core::ResourceManager& resources = Core::ResourceManager::GetInstance();
                Rendering::Cubemap* cubemap = resources.LoadCubemap(skyboxPath);
                if (cubemap) {
                    scene->SetSkyboxCubemap(cubemap);
                }
            }

            // Get gameObjects array
            lua_getfield(L, -1, "gameObjects");
            if (lua_istable(L, -1)) {
                int gameObjectsCount = luaL_len(L, -1);
                std::vector<std::pair<ECS::GameObject*, std::string>> parentingRequests;
                std::vector<UUIDRefRequest> uuidRefRequests;

                for (int i = 1; i <= gameObjectsCount; i++) {
                    lua_geti(L, -1, i);  // Push gameObjects[i]

                    if (lua_istable(L, -1)) {
                        ECS::GameObject* go = ProcessGameObject(L, lua_gettop(L), scene, parentingRequests, uuidRefRequests);
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
                        RTB_WARN("SceneLoader: Warning - Parent '" + parentName + "' not found for object '" + child->GetName() + "'");
                    }
                }

                for (const auto& req : uuidRefRequests) {
                    const std::string& uuidStr = req.uuidString;
                    size_t slash = uuidStr.find('/');

                    if (req.prop->type == Reflection::PropertyType::GameObjectRef) {
                        ECS::GameObject* target = scene->FindGameObjectByUUID(uuidStr);
                        if (target) {
                            void* data = (char*)req.component + req.prop->offset;
                            *(ECS::GameObject**)data = target;
                        } else {
                            RTB_WARN("SceneLoader: GameObjectRef UUID not found: " + uuidStr);
                        }
                    }
                    else if (req.prop->type == Reflection::PropertyType::ComponentRef) {
                        if (slash != std::string::npos) {
                            std::string uuid = uuidStr.substr(0, slash);
                            std::string type = uuidStr.substr(slash + 1);
                            ECS::GameObject* targetGO = scene->FindGameObjectByUUID(uuid);
                            if (targetGO) {
                                for (const auto& comp : targetGO->GetComponents()) {
                                    if (std::string(comp->GetTypeName()) == type) {
                                        void* data = (char*)req.component + req.prop->offset;
                                        *(ECS::Component**)data = comp.get();
                                        break;
                                    }
                                }
                            } else {
                                RTB_WARN("SceneLoader: ComponentRef UUID not found: " + uuid);
                            }
                        }
                    }
                }
            }
            lua_pop(L, 1);  // Pop gameObjects array

            lua_close(L);
            return scene;
        }

        ECS::GameObject* SceneLoader::ProcessGameObject(lua_State* L, int tableIndex, ECS::Scene* scene, std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests, std::vector<UUIDRefRequest>& uuidRefRequests) {

            // Read name
            lua_getfield(L, tableIndex, "name");
            std::string name = "Unnamed";
            if (lua_isstring(L, -1)) {
                name = lua_tostring(L, -1);
            }
            lua_pop(L, 1);

            // Create GameObject
            ECS::GameObject* go = new ECS::GameObject(name);

            // Restore UUID if present (skip regeneration)
            std::string savedUUID = SceneParsingUtils::ReadOptionalString(L, tableIndex, "uuid", "");
            if (!savedUUID.empty()) {
                go->SetUUID(savedUUID);
            }

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
            std::string parentName = SceneParsingUtils::ReadOptionalString(L, tableIndex, "parent", "");
            if (!parentName.empty()) {
                parentingRequests.push_back({ go, parentName });
            }

            // Process components
            lua_getfield(L, tableIndex, "components");
            if (lua_istable(L, -1)) {
                ProcessComponents(L, lua_gettop(L), go, uuidRefRequests);
            }
            lua_pop(L, 1);

            // Process inline children (format written by SceneSaver)
            lua_getfield(L, tableIndex, "children");
            if (lua_istable(L, -1)) {
                int childCount = luaL_len(L, -1);
                for (int c = 1; c <= childCount; c++) {
                    lua_geti(L, -1, c);
                    if (lua_istable(L, -1)) {
                        ECS::GameObject* child = ProcessGameObject(L, lua_gettop(L), scene, parentingRequests, uuidRefRequests);
                        if (child) {
                            scene->AddGameObject(child);
                            child->SetParent(go);
                        }
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);

            return go;
        }

        void SceneLoader::ProcessComponents(lua_State* L, int arrayIndex, ECS::GameObject* gameObject, std::vector<UUIDRefRequest>& uuidRefRequests) {
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
                                SceneComponentConfigurator::ConfigureMeshRenderer(L, componentTableIndex, static_cast<ECS::MeshRenderer*>(comp));
                            }
                            else if (componentType == "LightComponent") {
                                SceneComponentConfigurator::ConfigureLightComponent(L, componentTableIndex, static_cast<ECS::LightComponent*>(comp));
                            }
                            else if (componentType == "AudioSourceComponent") {
                                SceneComponentConfigurator::ConfigureAudioSource(L, componentTableIndex, static_cast<ECS::AudioSourceComponent*>(comp));
                            }
                            else if (componentType == "RigidBodyComponent") {
                                SceneComponentConfigurator::ConfigureRigidBody(L, componentTableIndex, static_cast<ECS::RigidBodyComponent*>(comp), gameObject);
                            }
                            else if (componentType == "BoxColliderComponent") {
                                SceneComponentConfigurator::ConfigureBoxCollider(L, componentTableIndex, static_cast<ECS::BoxColliderComponent*>(comp), gameObject);
                            }
                            else if (componentType == "Canvas") {
                                SceneComponentConfigurator::ConfigureCanvas(L, componentTableIndex, static_cast<UI::Canvas*>(comp));
                            }
                            else if (componentType == "UIText") {
                                SceneComponentConfigurator::ConfigureUIText(L, componentTableIndex, static_cast<UI::UIText*>(comp));
                            }
                            else if (componentType == "UIImage") {
                                SceneComponentConfigurator::ConfigureUIImage(L, componentTableIndex, static_cast<UI::UIImage*>(comp));
                            }
                            else if (componentType == "UIPanel") {
                                SceneComponentConfigurator::ConfigureUIPanel(L, componentTableIndex, static_cast<UI::UIPanel*>(comp));
                            }
                            else if (componentType == "UIButton") {
                                SceneComponentConfigurator::ConfigureUIButton(L, componentTableIndex, static_cast<UI::UIButton*>(comp));
                            }
                            else if (componentType == "UIContainer") {
                                SceneComponentConfigurator::SyncUIElementProxies(L, componentTableIndex, static_cast<UI::UIElement*>(comp));
                            }
                            else if (componentType == "CameraComponent") {
                                SceneComponentConfigurator::ConfigureCameraComponent(L, componentTableIndex, static_cast<ECS::CameraComponent*>(comp));
                            }
                            else if (componentType == "FreeLookCamera") {
                                SceneComponentConfigurator::ConfigureFreeLookCamera(L, componentTableIndex, static_cast<ECS::FreeLookCamera*>(comp));
                            }
                            else if (componentType == "Animator") {
                                SceneComponentConfigurator::ConfigureAnimator(L, componentTableIndex, static_cast<Animation::Animator*>(comp));
                            }

                            // Collect GameObjectRef / ComponentRef for deferred UUID resolution
                            const Reflection::TypeInfo* typeInfo = comp->GetTypeInfo();
                            if (typeInfo) {
                                for (const auto* prop : typeInfo->GetSerializableProperties()) {
                                    if (prop->type == Reflection::PropertyType::GameObjectRef ||
                                        prop->type == Reflection::PropertyType::ComponentRef)
                                    {
                                        lua_getfield(L, componentTableIndex, prop->name.c_str());
                                        if (lua_isstring(L, -1)) {
                                            std::string uuidStr = lua_tostring(L, -1);
                                            if (!uuidStr.empty())
                                                uuidRefRequests.push_back({ comp, prop, uuidStr });
                                        }
                                        lua_pop(L, 1);
                                    }
                                }
                            }
                        }
                        else {
                            RTB_ERROR("SceneLoader: Failed to create component '" + componentType + "'");
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
