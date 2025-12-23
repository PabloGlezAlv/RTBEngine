#pragma once
#include "../Math/Vectors/Vector2.h"
#include <vector>

struct SDL_Window;

namespace RTBEngine {
	namespace ECS {
		class Scene;
	}

	namespace UI {
		class Canvas;

		class CanvasSystem {
		public:
			static CanvasSystem& GetInstance() {
				static CanvasSystem instance;
				return instance;
			}

			CanvasSystem(const CanvasSystem&) = delete;
			CanvasSystem& operator=(const CanvasSystem&) = delete;

			bool Initialize(SDL_Window* window);
			void Shutdown();

			void Update(ECS::Scene* scene);
			void RenderAll();

			Math::Vector2 GetScreenSize() const { return screenSize; }

		private:
			CanvasSystem() = default;
			~CanvasSystem() = default;

			SDL_Window* window = nullptr;
			Math::Vector2 screenSize;
			std::vector<Canvas*> activeCanvases;
			bool isInitialized = false;
		};

	}
}
