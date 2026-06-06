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
            RTB_PROPERTY(faceCamera)
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
				ECS::GameObject* elementObject = element ? element->GetOwner() : nullptr;
				if (!elementObject || !elementObject->IsActiveInHierarchy()) {
					continue;
				}

				if (element->IsVisible() && element->IsEnabled()) {
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
				if (!obj || !obj->IsActiveInHierarchy()) {
					return;
				}

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
				if (!obj || !obj->IsActiveInHierarchy()) {
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
			if (!owner) {
				return;
			}

			const Math::Vector2 rootSize =
				renderMode == RenderMode::WorldSpace ? canvasSize : screenSize;

			std::function<void(ECS::GameObject*, const Math::Vector2&, const Math::Vector2&, const Math::Vector2&)> updateRecursive =
				[&](ECS::GameObject* obj, const Math::Vector2& parentWorldPos, const Math::Vector2& parentWorldSize, const Math::Vector2& parentLossyScale) {
					if (!obj || !obj->IsActiveInHierarchy()) {
						return;
					}

					Math::Vector2 childParentPos = parentWorldPos;
					Math::Vector2 childParentSize = parentWorldSize;
					Math::Vector2 childParentLossyScale = parentLossyScale;

					if (obj != owner) {
						UIElement* uiElement = obj->GetComponent<UIElement>();
						if (uiElement) {
							RectTransform* rectTransform = uiElement->GetRectTransform();
							if (rectTransform) {
								rectTransform->CalculateWorldTransform(parentWorldPos, parentWorldSize, parentLossyScale);
								childParentPos = rectTransform->GetWorldPosition();
								childParentSize = rectTransform->GetWorldSize();
								childParentLossyScale = rectTransform->GetLossyScale();
							}
						}
					}

					for (ECS::GameObject* child : obj->GetChildren()) {
						updateRecursive(child, childParentPos, childParentSize, childParentLossyScale);
					}
				};

			const Math::Vector2 rootLossyScale(1.0f, 1.0f);
			const Math::Vector2 rootPos(0.0f, 0.0f);
			for (ECS::GameObject* child : owner->GetChildren()) {
				updateRecursive(child, rootPos, rootSize, rootLossyScale);
			}
		}

	}
}
