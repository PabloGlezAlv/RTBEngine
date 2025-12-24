#include "SceneLoader.h"
#include "ComponentRegistry.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/Component.h"
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

                if (lua_istable(L, -1)) {
                    // Read component type
                    lua_getfield(L, -1, "type");
                    if (lua_isstring(L, -1)) {
                        std::string componentType = lua_tostring(L, -1);

                        // Create component using registry
                        ECS::Component* comp = ComponentRegistry::GetInstance().CreateComponent(componentType);
                        if (comp) {
                            gameObject->AddComponent(comp);

                            // TODO: Read component-specific properties from Lua
                            // For now, components are created with default values
                        }
                        else {
                            printf("SceneLoader: Failed to create component '%s'\n", componentType.c_str());
                        }
                    }
                    lua_pop(L, 1);  // Pop type
                }

                lua_pop(L, 1);  // Pop components[i]
            }
        }

    }
}
