#pragma once
#include <SDL.h>
#include <memory>
#include "Window.h"
#include "ApplicationConfig.h"

namespace RTBEngine {
	namespace ECS {
		class SceneManager;
		class Scene;
	}

	namespace Rendering {
		class Camera;
		class Shader;
		class Skybox;
	}

	namespace Physics {
		class PhysicsSystem;
		class PhysicsWorld;
	}
}

namespace RTBEngine {
	namespace Core {
		class Application {
		public:
			explicit Application(const ApplicationConfig& config);
			~Application();

			bool Initialize();
			void Run();
			void Shutdown();

			bool IsRunning() const { return isRunning; }
			void RequestExit() { isRunning = false; }
			Window* GetWindow() { return window.get(); }
			const ApplicationConfig& GetConfig() const { return config; }

			void ProcessInput();
			void Update(float deltaTime);
			void Render();

			void RenderShadowPass(ECS::Scene* scene);
			void RenderGeometryPass(ECS::Scene* scene, Rendering::Camera* camera);
			void SetIsRunning(bool value) { isRunning = value; }

		private:
			void RenderSceneDepthOnly(ECS::Scene* scene, Rendering::Shader* shader);
			ApplicationConfig config;

			bool isRunning = false;
			Uint32 lastTime = 0;
			float deltaTime = 0.0f;

			std::unique_ptr<Window> window;

			Physics::PhysicsWorld* physicsWorld;
			Physics::PhysicsSystem* physicsSystem;

			float physicsAccumulator = 0.0f;

			Rendering::Skybox* skybox = nullptr;

			Application(const Application&) = delete;
			Application& operator=(const Application&) = delete;
		};
	}

	// Helper functionto simply run the program
	inline int Run(const Core::ApplicationConfig& config) {
		Core::Application app(config);
		if (!app.Initialize()) return -1;
		app.Run();
		return 0;
	}
}
