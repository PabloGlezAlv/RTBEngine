#pragma once
#include "../Math/Vectors/Vector2.h"
#include "../Math/Vectors/Vector4.h"
#include "EventSystem/PointerEventData.h"
#include <vector>

struct SDL_Window;

namespace RTBEngine {
	namespace ECS {
		class Scene;
		class GameObject;
	}

	namespace UI {
		class Canvas;
		class UIElement;

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
			void RenderCanvasesOnly(const Math::Vector2* customScreenSize = nullptr);
			void RenderCanvasesToDrawList(void* drawList, const Math::Vector2& screenSize, const Math::Vector2& offset);
			void InitializeFonts();
			void ProcessInput();

			Math::Vector2 GetScreenSize() const { return screenSize; }
			Math::Vector2 GetMousePosition() const;

		private:
			CanvasSystem() = default;
			~CanvasSystem() = default;

			UIElement* GetElementUnderMouse(const Math::Vector2& mousePos);
			bool IsPointInRect(const Math::Vector2& point, const Math::Vector4& rect);

			template<typename THandler, typename TCallback>
			void ExecuteEvents(ECS::GameObject* target, const PointerEventData& eventData, TCallback callback);

			SDL_Window* window = nullptr;
			Math::Vector2 screenSize;
			std::vector<Canvas*> activeCanvases;
			bool isInitialized = false;

			ECS::GameObject* hoveredGameObject = nullptr;
			ECS::GameObject* pressedGameObject = nullptr;
		};

	}
}
