#include "Canvas.h"
#include "../ECS/GameObject.h"

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

			// Get all children GameObjects
			const auto& children = owner->GetChildren();
			for (ECS::GameObject* child : children) {
				// Check if child has a UIElement component
				UIElement* uiElem = child->GetComponent<UIElement>();
				if (uiElem) {
					cachedUIElements.push_back(uiElem);
				}

				// Recursively check grandchildren
				const auto& grandchildren = child->GetChildren();
				for (ECS::GameObject* grandchild : grandchildren) {
					UIElement* grandElem = grandchild->GetComponent<UIElement>();
					if (grandElem) {
						cachedUIElements.push_back(grandElem);
					}
				}
			}
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
