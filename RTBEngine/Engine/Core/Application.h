#pragma once
#include <SDL.h>
#include <memory>
#include "Window.h"

namespace RTBEngine {
	namespace ECS
	{
		class SceneManager;
	}

	namespace Physics
	{
		class PhysicsSystem;
		class PhysicsWorld;
	}
}

namespace RTBEngine {
	namespace Core {
		class Application
		{
		public:
			Application();
			~Application();

			bool Initialize();

			void Run();

			void Shutdown();
		private:
			void ProcessInput();
			void Update(float deltaTime);
			void Render();
		private:
			bool isRunning = false;
			Uint32 lastTime = 0;
			float deltaTime = 0.0f;

			std::unique_ptr<Window> window;

			Physics::PhysicsWorld* physicsWorld;
			Physics::PhysicsSystem* physicsSystem;

			const float PHYSICS_TIMESTEP = 1.0f / 60.0f;
			float physicsAccumulator = 0.0f;

			Application(const Application&) = delete;
			Application& operator=(const Application&) = delete;
		};
	}
}
