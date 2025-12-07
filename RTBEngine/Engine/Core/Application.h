#pragma once
#include <SDL.h>
#include "Window.h"

namespace RTBEngine {
	namespace Rendering {
		class Shader;
		class Camera;
		class Mesh;
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

			Window* window;
			Rendering::Camera* camera;

			Rendering::Shader* testShader;
			Rendering::Mesh* testMesh;

			Application(const Application&) = delete;
			Application& operator=(const Application&) = delete;
		};
	}
}

