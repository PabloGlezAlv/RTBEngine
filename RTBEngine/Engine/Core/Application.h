#pragma once
#include <SDL.h>
#include "Window.h"

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
			Uint32 lastTime;
			float deltaTime = 0.0f;

			Window* window;

			Application(const Application&) = delete;
			Application& operator=(const Application&) = delete;
		};
	}
}

