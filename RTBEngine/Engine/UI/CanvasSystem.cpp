#include "CanvasSystem.h"
#include "Canvas.h"
#include "UIElement.h"
#include "UIRenderContext.h"
#include "EventSystem/IPointerEnterHandler.h"
#include "EventSystem/IPointerExitHandler.h"
#include "EventSystem/IPointerDownHandler.h"
#include "EventSystem/IPointerUpHandler.h"
#include "EventSystem/IPointerClickHandler.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../Input/InputManager.h"
#include "../Input/MouseButton.h"
#include <imgui.h>
#include <algorithm>

namespace RTBEngine {
	namespace UI {

		void CanvasSystem::Update(ECS::Scene* scene) {
			if (!scene) return;

			activeScene = scene;
			activeCanvases.clear();

			for (const auto& objPtr : scene->GetGameObjects()) {
				ECS::GameObject* obj = objPtr.get();
				Canvas* canvas = obj->GetComponent<Canvas>();
				if (canvas && canvas->IsEnabled() && obj->IsActive()) {
					activeCanvases.push_back(canvas);
				}
			}

			std::sort(activeCanvases.begin(), activeCanvases.end(),
				[](Canvas* a, Canvas* b) {
					return a->GetSortOrder() < b->GetSortOrder();
				});
		}

		void CanvasSystem::UpdateAllRectTransforms(const Math::Vector2& customScreenSize) {
			screenSize = customScreenSize;
			for (Canvas* canvas : activeCanvases) {
				canvas->PrepareForHitTest(customScreenSize);
			}
		}

		void CanvasSystem::RenderToDrawList(ImDrawList* drawList, const Math::Vector2& renderScreenSize, const Math::Vector2& offset) {
			UIRenderContext::Begin(drawList, offset);

			for (Canvas* canvas : activeCanvases) {
				canvas->RenderCanvas(renderScreenSize);
			}

			UIRenderContext::End();
		}

		bool CanvasSystem::IsGameObjectAlive(ECS::GameObject* gameObject) const {
			if (!activeScene || !gameObject) return false;
			for (const auto& obj : activeScene->GetGameObjects()) {
				if (obj.get() == gameObject) return true;
			}
			return false;
		}

		std::vector<Math::Vector4> CanvasSystem::GetRaycastRectsForGameObject(ECS::GameObject* gameObject) const {
			std::vector<Math::Vector4> rects;
			if (!gameObject) return rects;
			for (Canvas* canvas : activeCanvases) {
				for (UIElement* element : canvas->GetUIElements()) {
					if (element->GetOwner() == gameObject && element->IsRaycastTarget()) {
						rects.push_back(element->GetRectTransform()->GetScreenRect());
					}
				}
			}
			return rects;
		}

		bool CanvasSystem::IsPointInRect(const Math::Vector2& point, const Math::Vector4& rect) {
			return point.x >= rect.x && point.x <= rect.x + rect.z &&
				   point.y >= rect.y && point.y <= rect.y + rect.w;
		}

		UIElement* CanvasSystem::GetElementUnderMouse(const Math::Vector2& mousePos) {
			for (auto it = activeCanvases.rbegin(); it != activeCanvases.rend(); ++it) {
				Canvas* canvas = *it;
				const auto& elements = canvas->GetUIElements();

				for (auto elemIt = elements.rbegin(); elemIt != elements.rend(); ++elemIt) {
					UIElement* element = *elemIt;
					if (!element->IsVisible() || !element->IsEnabled() || !element->IsRaycastTarget()) continue;

					Math::Vector4 screenRect = element->GetRectTransform()->GetScreenRect();
					if (IsPointInRect(mousePos, screenRect)) {
						return element;
					}
				}
			}
			return nullptr;
		}

		template<typename THandler, typename TCallback>
		void CanvasSystem::ExecuteEvents(ECS::GameObject* target, const PointerEventData& eventData, TCallback callback) {
			if (!target) return;

			for (const auto& comp : target->GetComponents()) {
				THandler* handler = dynamic_cast<THandler*>(comp.get());
				if (handler) {
					callback(handler, eventData);
				}
			}
		}

		void CanvasSystem::ProcessInput(const Math::Vector2& mousePos) {
			if (!IsGameObjectAlive(hoveredGameObject)) hoveredGameObject = nullptr;
			if (!IsGameObjectAlive(pressedGameObject)) pressedGameObject = nullptr;

			Input::InputManager& input = Input::InputManager::GetInstance();

			UIElement* elementUnderMouse = GetElementUnderMouse(mousePos);
			ECS::GameObject* currentGO = elementUnderMouse ? elementUnderMouse->GetOwner() : nullptr;

			PointerEventData eventData;
			eventData.position = mousePos;
			eventData.pointerEnter = currentGO;

			if (hoveredGameObject != currentGO) {
				if (hoveredGameObject) {
					ExecuteEvents<IPointerExitHandler>(hoveredGameObject, eventData,
						[](IPointerExitHandler* h, const PointerEventData& e) { h->OnPointerExit(e); });
				}

				if (currentGO) {
					ExecuteEvents<IPointerEnterHandler>(currentGO, eventData,
						[](IPointerEnterHandler* h, const PointerEventData& e) { h->OnPointerEnter(e); });
				}

				hoveredGameObject = currentGO;
			}

			if (input.IsMouseButtonJustPressed(Input::MouseButton::Left)) {
				if (currentGO) {
					pressedGameObject = currentGO;
					eventData.pointerPress = currentGO;
					ExecuteEvents<IPointerDownHandler>(currentGO, eventData,
						[](IPointerDownHandler* h, const PointerEventData& e) { h->OnPointerDown(e); });
				}
			}

			if (input.IsMouseButtonJustReleased(Input::MouseButton::Left)) {
				eventData.pointerPress = pressedGameObject;

				ECS::GameObject* releaseTarget = pressedGameObject ? pressedGameObject : currentGO;
				if (releaseTarget) {
					ExecuteEvents<IPointerUpHandler>(releaseTarget, eventData,
						[](IPointerUpHandler* h, const PointerEventData& e) { h->OnPointerUp(e); });
				}

				if (pressedGameObject && pressedGameObject == currentGO) {
					ExecuteEvents<IPointerClickHandler>(pressedGameObject, eventData,
						[](IPointerClickHandler* h, const PointerEventData& e) { h->OnPointerClick(e); });
				}
				pressedGameObject = nullptr;
			}
		}

	}
}
