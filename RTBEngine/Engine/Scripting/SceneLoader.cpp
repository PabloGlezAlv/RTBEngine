#include "SceneLoader.h"
#include "ComponentRegistry.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/Component.h"
#include "../ECS/MissingComponent.h"
#include "../ECS/MeshRenderer.h"
#include "../ECS/LightComponent.h"
#include "../ECS/AudioSourceComponent.h"
#include "../ECS/RigidBodyComponent.h"
#include "../ECS/BoxColliderComponent.h"
#include "../ECS/Prefab.h"
#include "../ECS/PrefabRegistry.h"
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
#include "SceneReflectionUtils.h"

namespace RTBEngine {
    namespace Scripting {

        void SceneLoader::SetupLuaBindings(lua_State* L) {
            SceneLuaBindings::SetupLuaBindings(L);
        }

        ECS::Scene* SceneLoader::LoadScene(const std::string& filePath) {
            lua_State* L = luaL_newstate();
            luaL_openlibs(L);
            SetupLuaBindings(L);

            if (luaL_dofile(L, filePath.c_str()) != LUA_OK) {
                RTB_ERROR("SceneLoader: Failed to load file '" + filePath + "': " + std::string(lua_tostring(L, -1)));
                lua_close(L);
                return nullptr;
            }

            lua_getglobal(L, "CreateScene");
            if (!lua_isfunction(L, -1)) {
                RTB_ERROR("SceneLoader: 'CreateScene' function not found in " + filePath);
                lua_close(L);
                return nullptr;
            }

            if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
                RTB_ERROR("SceneLoader: Error calling CreateScene(): " + std::string(lua_tostring(L, -1)));
                lua_close(L);
                return nullptr;
            }

            if (!SceneParsingUtils::ValidateSceneTable(L, -1, filePath)) {
                lua_close(L);
                return nullptr;
            }

            const std::string sceneName = SceneParsingUtils::ReadOptionalString(L, -1, "name", "Unnamed Scene");
            ECS::Scene* scene = new ECS::Scene(sceneName);

            std::string skyboxPath = SceneParsingUtils::ReadOptionalString(L, -1, "skybox", "");
            bool skyboxEnabled = SceneParsingUtils::ReadOptionalBool(L, -1, "skyboxEnabled", true);
            scene->SetSkyboxEnabled(skyboxEnabled);
            if (!skyboxPath.empty()) {
                Rendering::Cubemap* cubemap = Core::ResourceManager::GetInstance().LoadCubemapAsset(skyboxPath);
                if (cubemap)
                    scene->SetSkyboxCubemap(cubemap);
            }

            std::vector<std::pair<ECS::GameObject*, std::string>> parentingRequests;
            std::vector<UUIDRefRequest> uuidRefRequests;

            lua_getfield(L, -1, "gameObjects");
            if (lua_istable(L, -1)) {
                int count = luaL_len(L, -1);
                for (int i = 1; i <= count; i++) {
                    lua_geti(L, -1, i);
                    if (lua_istable(L, -1)) {
                        ECS::GameObject* go = ProcessGameObject(L, lua_gettop(L), scene, parentingRequests, uuidRefRequests);
                        if (go)
                            scene->AddGameObject(go);
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);

            ResolveParenting(scene, parentingRequests);
            ResolveUUIDRefs(scene, uuidRefRequests);

            lua_close(L);
            return scene;
        }

        ECS::GameObject* SceneLoader::ProcessGameObject(lua_State* L, int tableIndex, ECS::Scene* scene,
            std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
            std::vector<UUIDRefRequest>& uuidRefRequests)
        {
            lua_getfield(L, tableIndex, "name");
            std::string name = lua_isstring(L, -1) ? lua_tostring(L, -1) : "Unnamed";
            lua_pop(L, 1);

            std::string savedUUID = SceneParsingUtils::ReadOptionalString(L, tableIndex, "uuid", "");
            std::string prefabName = SceneParsingUtils::ReadOptionalString(L, tableIndex, "prefab", "");

            if (!prefabName.empty())
                return ProcessPrefabInstance(L, tableIndex, scene, name, savedUUID, uuidRefRequests);

            ECS::GameObject* go = new ECS::GameObject(name);
            if (!savedUUID.empty())
                go->SetUUID(savedUUID);

            ReadTransform(L, tableIndex, go);

            std::string parentName = SceneParsingUtils::ReadOptionalString(L, tableIndex, "parent", "");
            if (!parentName.empty())
                parentingRequests.push_back({ go, parentName });

            lua_getfield(L, tableIndex, "components");
            if (lua_istable(L, -1))
                ProcessComponents(L, lua_gettop(L), go, uuidRefRequests);
            lua_pop(L, 1);

            ProcessChildren(L, tableIndex, scene, go, parentingRequests, uuidRefRequests);

            return go;
        }

        ECS::GameObject* SceneLoader::ProcessPrefabInstance(lua_State* L, int tableIndex, ECS::Scene* scene,
            const std::string& name, const std::string& uuid,
            std::vector<UUIDRefRequest>& uuidRefRequests)
        {
            std::string prefabName = SceneParsingUtils::ReadOptionalString(L, tableIndex, "prefab", "");
            const ECS::Prefab* prefab = ECS::PrefabRegistry::GetInstance().Get(prefabName);
            if (!prefab) {
                RTB_WARN("SceneLoader: Prefab '" + prefabName + "' not found — creating empty GO");
                return new ECS::GameObject(name);
            }

            ECS::GameObject* go = prefab->Instantiate(nullptr);
            go->SetName(name);
            if (!uuid.empty())
                go->SetUUID(uuid);

            ReadTransform(L, tableIndex, go);

            lua_getfield(L, tableIndex, "overrides");
            if (lua_istable(L, -1)) {
                int overridesIndex = lua_gettop(L);
                lua_getfield(L, overridesIndex, "components");
                if (lua_istable(L, -1))
                    ProcessComponents(L, lua_gettop(L), go, uuidRefRequests);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);

            return go;
        }

        void SceneLoader::ReadTransform(lua_State* L, int tableIndex, ECS::GameObject* go) {
            lua_getfield(L, tableIndex, "position");
            if (lua_isuserdata(L, -1)) {
                auto result = luabridge::Stack<Math::Vector3>::get(L, -1);
                if (result) go->GetTransform().SetPosition(result.value());
            }
            lua_pop(L, 1);

            lua_getfield(L, tableIndex, "rotation");
            if (lua_isuserdata(L, -1)) {
                auto result = luabridge::Stack<Math::Quaternion>::get(L, -1);
                if (result) go->GetTransform().SetRotation(result.value());
            }
            lua_pop(L, 1);

            lua_getfield(L, tableIndex, "scale");
            if (lua_isuserdata(L, -1)) {
                auto result = luabridge::Stack<Math::Vector3>::get(L, -1);
                if (result) go->GetTransform().SetScale(result.value());
            }
            lua_pop(L, 1);
        }

        void SceneLoader::ProcessChildren(lua_State* L, int tableIndex, ECS::Scene* scene,
            ECS::GameObject* parent,
            std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
            std::vector<UUIDRefRequest>& uuidRefRequests)
        {
            lua_getfield(L, tableIndex, "children");
            if (lua_istable(L, -1)) {
                int count = luaL_len(L, -1);
                for (int c = 1; c <= count; c++) {
                    lua_geti(L, -1, c);
                    if (lua_istable(L, -1)) {
                        ECS::GameObject* child = ProcessGameObject(L, lua_gettop(L), scene, parentingRequests, uuidRefRequests);
                        if (child) {
                            scene->AddGameObject(child);
                            child->SetParent(parent);
                        }
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }

        void SceneLoader::ResolveParenting(ECS::Scene* scene,
            const std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests)
        {
            for (const auto& req : parentingRequests) {
                ECS::GameObject* parent = scene->FindGameObject(req.second);
                if (parent)
                    req.first->SetParent(parent);
                else
                    RTB_WARN("SceneLoader: Parent '" + req.second + "' not found for '" + req.first->GetName() + "'");
            }
        }

        void SceneLoader::ResolveUUIDRefs(ECS::Scene* scene,
            const std::vector<RTBEngine::Scripting::SceneLoader::UUIDRefRequest>& uuidRefRequests)
        {
            for (const auto& req : uuidRefRequests) {
                const std::string& uuidStr = req.uuidString;

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
                    size_t slash = uuidStr.find('/');
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

        void SceneLoader::ProcessComponents(lua_State* L, int arrayIndex, ECS::GameObject* gameObject,
            std::vector<UUIDRefRequest>& uuidRefRequests)
        {
            int componentCount = luaL_len(L, arrayIndex);

            for (int i = 1; i <= componentCount; i++) {
                lua_geti(L, arrayIndex, i);
                int componentTableIndex = lua_gettop(L);

                if (!lua_istable(L, -1)) {
                    lua_pop(L, 1);
                    continue;
                }

                lua_getfield(L, -1, "type");
                if (!lua_isstring(L, -1)) {
                    lua_pop(L, 2);
                    continue;
                }

                std::string componentType = lua_tostring(L, -1);
                lua_pop(L, 1);

                ECS::Component* comp = ComponentRegistry::GetInstance().CreateComponent(componentType);
                if (!comp) {
                    RTB_ERROR("SceneLoader: Component type '" + componentType + "' not found — inserting MissingComponent placeholder");
                    gameObject->AddComponent(new ECS::MissingComponent(componentType));
                    lua_pop(L, 1);
                    continue;
                }

                // If the GO already has this component (from prefab), reuse it instead of adding a duplicate
                ECS::Component* existing = nullptr;
                for (const auto& c : gameObject->GetComponents()) {
                    if (std::string(c->GetTypeName()) == componentType) {
                        existing = c.get();
                        break;
                    }
                }

                if (existing) {
                    delete comp;
                    comp = existing;
                } else {
                    gameObject->AddComponent(comp);
                }

                SceneReflectionUtils::ApplyLuaTableToComponent(L, componentTableIndex, comp);

                if (componentType == "MeshRenderer")
                    SceneComponentConfigurator::ConfigureMeshRenderer(L, componentTableIndex, static_cast<ECS::MeshRenderer*>(comp));
                else if (componentType == "LightComponent")
                    SceneComponentConfigurator::ConfigureLightComponent(L, componentTableIndex, static_cast<ECS::LightComponent*>(comp));
                else if (componentType == "AudioSourceComponent")
                    SceneComponentConfigurator::ConfigureAudioSource(L, componentTableIndex, static_cast<ECS::AudioSourceComponent*>(comp));
                else if (componentType == "RigidBodyComponent")
                    SceneComponentConfigurator::ConfigureRigidBody(L, componentTableIndex, static_cast<ECS::RigidBodyComponent*>(comp), gameObject);
                else if (componentType == "BoxColliderComponent")
                    SceneComponentConfigurator::ConfigureBoxCollider(L, componentTableIndex, static_cast<ECS::BoxColliderComponent*>(comp), gameObject);
                else if (componentType == "Canvas")
                    SceneComponentConfigurator::ConfigureCanvas(L, componentTableIndex, static_cast<UI::Canvas*>(comp));
                else if (componentType == "UIText")
                    SceneComponentConfigurator::ConfigureUIText(L, componentTableIndex, static_cast<UI::UIText*>(comp));
                else if (componentType == "UIImage")
                    SceneComponentConfigurator::ConfigureUIImage(L, componentTableIndex, static_cast<UI::UIImage*>(comp));
                else if (componentType == "UIPanel")
                    SceneComponentConfigurator::ConfigureUIPanel(L, componentTableIndex, static_cast<UI::UIPanel*>(comp));
                else if (componentType == "UIButton")
                    SceneComponentConfigurator::ConfigureUIButton(L, componentTableIndex, static_cast<UI::UIButton*>(comp));
                else if (componentType == "UIContainer")
                    SceneComponentConfigurator::SyncUIElementProxies(L, componentTableIndex, static_cast<UI::UIElement*>(comp));
                else if (componentType == "CameraComponent")
                    SceneComponentConfigurator::ConfigureCameraComponent(L, componentTableIndex, static_cast<ECS::CameraComponent*>(comp));
                else if (componentType == "FreeLookCamera")
                    SceneComponentConfigurator::ConfigureFreeLookCamera(L, componentTableIndex, static_cast<ECS::FreeLookCamera*>(comp));
                else if (componentType == "Animator")
                    SceneComponentConfigurator::ConfigureAnimator(L, componentTableIndex, static_cast<Animation::Animator*>(comp));

                comp->OnValidate();

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

                lua_pop(L, 1);
            }
        }

    }
}
