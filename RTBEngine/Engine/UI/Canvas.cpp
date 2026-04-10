#include "Canvas.h"
#include "UIElement.h"
#include "../ECS/GameObject.h"
#include <functional>

namespace RTBEngine {
	namespace UI {

        using ThisClass = Canvas;
        RTB_REGISTER_COMPONENT(Canvas)
            RTB_PROPERTY_ENUM(renderMode, "ScreenSpaceOverlay", "ScreenSpaceCamera", "WorldSpace")
            RTB_PROPERTY(canvasSize)
            RTB_PROPERTY(sortOrder)
        RTB_END_REGISTER(Canvas)

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

		void Canvas::PrepareForHitTest(const Math::Vector2& screenSize) {
			if (!owner) return;

			CollectUIElements();

			for (UIElement* element : cachedUIElements) {
				if (element) element->SyncRectTransform();
			}

			UpdateRectTransforms(screenSize);
		}

		void Canvas::RenderCanvas(const Math::Vector2& screenSize) {
			if (!owner) return;

			PrepareForHitTest(screenSize);

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

				// Default: canvas is the "root" parent with scale 1.0
				Math::Vector2 parentWorldPos(0.0f, 0.0f);
				Math::Vector2 parentWorldSize = screenSize;
				Math::Vector2 parentLossyScale(1.0f, 1.0f);

				if (parentObj && parentObj != owner) {
					UIElement* parentUI = parentObj->GetComponent<UIElement>();
					if (parentUI && parentUI->GetRectTransform()) {
						RectTransform* parentRT = parentUI->GetRectTransform();
						// KEY FIX: Use parent's WORLD values (scaled) not layout values (unscaled)
						parentWorldPos = parentRT->GetWorldPosition();
						parentWorldSize = parentRT->GetWorldSize();
						parentLossyScale = parentRT->GetLossyScale();
					}
				}

				// Calculate this element's world transform using parent's world values
				rt->CalculateWorldTransform(parentWorldPos, parentWorldSize, parentLossyScale);
			}
		}

	}
}
