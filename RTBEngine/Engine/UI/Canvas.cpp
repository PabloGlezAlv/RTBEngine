#include "Canvas.h"
#include "../ECS/GameObject.h"
#include <functional>

namespace RTBEngine {
	namespace UI {

		Canvas::Canvas() {
			canvasSize = Math::Vector2(1920.0f, 1080.0f);
		}

		Canvas::~Canvas() {
		}

		void Canvas::OnAwake() {
			isInitialized = true;
		}

		void Canvas::OnStart() {
			CollectUIElements();
		}

		void Canvas::OnUpdate(float deltaTime) {
			// Recollect UI elements (in case children were added/removed)
			CollectUIElements();
		}

		void Canvas::OnDestroy() {
			cachedUIElements.clear();
			isInitialized = false;
		}

		void Canvas::RenderCanvas(const Math::Vector2& screenSize) {
			if (!isInitialized) return;

			UpdateRectTransforms(screenSize);

			for (UIElement* element : cachedUIElements) {
				if (element && element->IsVisible() && element->IsEnabled()) {
					element->Render();
				}
			}
		}

		void Canvas::CollectUIElements() {
			cachedUIElements.clear();

			if (!owner) return;

			// Helper for recursive collection
			std::function<void(ECS::GameObject*)> collectRecursive = [&](ECS::GameObject* obj) {
				UIElement* uiElem = obj->GetComponent<UIElement>();
				if (uiElem && uiElem->GetOwner() != owner) {
					cachedUIElements.push_back(uiElem);
				}

				for (ECS::GameObject* child : obj->GetChildren()) {
					collectRecursive(child);
				}
			};

			collectRecursive(owner);
		}

		void Canvas::UpdateRectTransforms(const Math::Vector2& screenSize) {
			for (UIElement* element : cachedUIElements) {
				if (!element) continue;

				RectTransform* rt = element->GetRectTransform();
				if (!rt) continue;

				ECS::GameObject* parentObj = element->GetOwner()->GetParent();
				Math::Vector2 parentPos(0.0f, 0.0f);
				Math::Vector2 parentSize = screenSize;

				if (parentObj && parentObj != owner) {
					UIElement* parentUI = parentObj->GetComponent<UIElement>();
					if (parentUI && parentUI->GetRectTransform()) {
						parentPos = parentUI->GetRectTransform()->GetScreenPosition();
						parentSize = parentUI->GetRectTransform()->GetScreenSize();
					}
				}

				rt->CalculateScreenRect(parentPos, parentSize);
			}
		}

	}
}
