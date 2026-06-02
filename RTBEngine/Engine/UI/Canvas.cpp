#include "Canvas.h"
#include "UIElement.h"
#include "Elements/UILayoutGroup.h"
#include "Elements/UIHorizontalLayout.h"
#include "Elements/UIVerticalLayout.h"
#include "../ECS/GameObject.h"
#include <algorithm>
#include <functional>
#include <cstdint>

namespace RTBEngine {
	namespace UI {

        using ThisClass = Canvas;
        RTB_REGISTER_COMPONENT(Canvas)
            RTB_PROPERTY_ENUM(renderMode, "ScreenSpaceOverlay", "ScreenSpaceCamera", "WorldSpace")
            RTB_PROPERTY(canvasSize)
			RTB_PROPERTY_RANGE(pixelsPerUnit, 1.0f, 1000.0f)
            RTB_PROPERTY(sortOrder)
        RTB_END_REGISTER(Canvas)

		Canvas::Canvas() {
			canvasSize = Math::Vector2(1920.0f, 1080.0f);
		}

		Canvas::~Canvas() {
		}

		void Canvas::SetPixelsPerUnit(float value) {
			pixelsPerUnit = std::max(1.0f, value);
		}

		void Canvas::OnAwake() {
			isInitialized = true;
			SetUpdateTickEnabled(false);
		}

		void Canvas::OnStart() {
			hierarchyDirty = true;
			CollectUIElementsIfNeeded();
		}

		void Canvas::OnDestroy() {
			cachedUIElements.clear();
			isInitialized = false;
		}

		void Canvas::PrepareForHitTest(const Math::Vector2& screenSize) {
			if (!owner) return;

			CollectUIElementsIfNeeded();
			ApplyLayoutGroups();
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

		void Canvas::CollectUIElementsIfNeeded() {
			uint32_t currentVersion = ECS::GameObject::GetHierarchyVersion();
			if (!hierarchyDirty && lastHierarchyVersion == currentVersion) return;
			hierarchyDirty = false;
			lastHierarchyVersion = currentVersion;

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

		void Canvas::ApplyLayoutGroups() {
			if (!owner) {
				return;
			}

			std::function<void(ECS::GameObject*)> applyRecursive = [&](ECS::GameObject* obj) {
				if (!obj) {
					return;
				}

				for (const auto& component : obj->GetComponents()) {
					if (!component) {
						continue;
					}

					if (const UIHorizontalLayout* horizontalLayout = dynamic_cast<const UIHorizontalLayout*>(component.get())) {
						horizontalLayout->ApplyLayout();
					} else if (const UIVerticalLayout* verticalLayout = dynamic_cast<const UIVerticalLayout*>(component.get())) {
						verticalLayout->ApplyLayout();
					}
				}

				for (ECS::GameObject* child : obj->GetChildren()) {
					applyRecursive(child);
				}
			};

			applyRecursive(owner);
		}

		void Canvas::UpdateRectTransforms(const Math::Vector2& screenSize) {
			const Math::Vector2 rootSize =
				renderMode == RenderMode::WorldSpace ? canvasSize : screenSize;

			for (UIElement* element : cachedUIElements) {
				if (!element) continue;

				RectTransform* rt = element->GetRectTransform();
				if (!rt) continue;

				ECS::GameObject* parentObj = element->GetOwner()->GetParent();

				// Default: canvas is the "root" parent with scale 1.0
				Math::Vector2 parentWorldPos(0.0f, 0.0f);
				Math::Vector2 parentWorldSize = rootSize;
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

				rt->CalculateWorldTransform(parentWorldPos, parentWorldSize, parentLossyScale);
			}
		}

	}
}
