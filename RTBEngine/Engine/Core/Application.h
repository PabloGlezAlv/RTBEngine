#pragma once

#include "../RTBEngineAPI.h"
#include "ApplicationConfig.h"
#include <cstdint>
#include <functional>
#include <memory>

namespace RTBEngine {
    namespace ECS {
        class GameObject;
        class Scene;
    }

    namespace Physics {
        class PhysicsSystem;
        class PhysicsWorld;
    }

    namespace Rendering {
        class Camera;
        class Frustum;
        class Shader;
        class Skybox;
    }
}

namespace RTBEngine {
    namespace Core {
        #pragma warning(push)
        #pragma warning(disable: 4251)
        class Window;

        class RTB_API Application {
        public:
            explicit Application(const ApplicationConfig& config);
            ~Application();

            bool Initialize();
            void Run();
            void Shutdown();

            bool IsRunning() const { return isRunning; }
            void RequestExit() { isRunning = false; }
            static void RequestQuit();
            static bool IsQuitRequested();
            static bool ConsumeQuitRequest();
            static void ClearQuitRequest();

            Window* GetWindow() { return window.get(); }
            void* GetImGuiContext();

            void SetOnQuitRequested(std::function<void()> callback) { onQuitRequested = callback; }
            const ApplicationConfig& GetConfig() const { return config; }

            void ProcessInput();
            void Update(float deltaTime);
            void Render();

            void RenderShadowPass(ECS::Scene* scene);
            void RenderGeometryPass(ECS::Scene* scene, Rendering::Camera* camera);
            void SetIsRunning(bool value) { isRunning = value; }

            void ResetPhysics();
            void RebuildPhysicsForScene(ECS::Scene* scene);
            void InitializePhysicsForScene(ECS::Scene* scene);
            Physics::PhysicsWorld* GetPhysicsWorld() const { return physicsWorld; }

        private:
            bool InitializeImGui();
            void ShutdownImGui();
            void RenderSceneDepthOnly(ECS::Scene* scene,
                                      Rendering::Shader* shader,
                                      const Rendering::Frustum& frustum);
            void OnWindowResized(int width, int height);
            void InitializePhysicsForGameObject(ECS::GameObject* gameObject);
            void InitializePhysicsForHierarchy(ECS::GameObject* root);
            void DetachPhysicsFromGameObject(ECS::GameObject* gameObject);
            void DetachPhysicsHierarchy(ECS::GameObject* root);

            ApplicationConfig config;
            bool isRunning = false;
            bool isShutdown = false;
            std::uint32_t lastTime = 0;

            std::unique_ptr<Window> window;
            Physics::PhysicsWorld* physicsWorld = nullptr;
            Physics::PhysicsSystem* physicsSystem = nullptr;
            Rendering::Skybox* skybox = nullptr;
            std::function<void()> onQuitRequested;

            Application(const Application&) = delete;
            Application& operator=(const Application&) = delete;
        };
        #pragma warning(pop)
    }

    inline int Run(const Core::ApplicationConfig& config) {
        Core::Application app(config);
        if (!app.Initialize()) {
            return -1;
        }

        app.Run();
        return 0;
    }
}
