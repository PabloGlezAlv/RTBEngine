#pragma once
#include "../Math/Vectors/Vector2.h"
#include "../Math/Vectors/Vector4.h"
#include "EventSystem/PointerEventData.h"
#include <vector>

struct ImDrawList;

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

			void Update(ECS::Scene* scene);
			void UpdateAllRectTransforms(const Math::Vector2& screenSize);
			void ProcessInput(const Math::Vector2& mousePos);
			void RenderToDrawList(ImDrawList* drawList, const Math::Vector2& screenSize, const Math::Vector2& offset);

			Math::Vector2 GetScreenSize() const { return screenSize; }
			std::vector<Math::Vector4> GetRaycastRectsForGameObject(ECS::GameObject* gameObject) const;

		private:
			CanvasSystem() = default;
			~CanvasSystem() = default;

			bool IsGameObjectAlive(ECS::GameObject* gameObject) const;
			UIElement* GetElementUnderMouse(const Math::Vector2& mousePos);
			bool IsPointInRect(const Math::Vector2& point, const Math::Vector4& rect);

			template<typename THandler, typename TCallback>
			void ExecuteEvents(ECS::GameObject* target, const PointerEventData& eventData, TCallback callback);

			Math::Vector2 screenSize;
			std::vector<Canvas*> activeCanvases;
			ECS::Scene* activeScene = nullptr;

			ECS::GameObject* hoveredGameObject = nullptr;
			ECS::GameObject* pressedGameObject = nullptr;
		};

	}
}
