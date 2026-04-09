#include "UIElement.h"
#include "../ECS/GameObject.h"

namespace RTBEngine {
	namespace UI {

		UIElement::UIElement() {
			rectTransform = std::make_unique<RectTransform>();
		}

		UIElement::~UIElement() {

		}

		void UIElement::SyncRectTransform() {
			if (!rectTransform) return;
			rectTransform->SetAnchorMin(anchorMin);
			rectTransform->SetAnchorMax(anchorMax);
			rectTransform->SetPivot(pivot);
			rectTransform->SetAnchoredPosition(anchoredPosition);
			rectTransform->SetSize(sizeDelta);
			rectTransform->SetRotation(rotation);
			rectTransform->SetScale(scale);

			// Propagate dirty to children since our transform changed
			PropagateDirtyToChildren();
		}

		void UIElement::PropagateDirtyToChildren() {
			if (!GetOwner()) return;

			// Recursively mark all child UIElement RectTransforms as dirty
			for (ECS::GameObject* child : GetOwner()->GetChildren()) {
				if (!child) continue;

				UIElement* childUI = child->GetComponent<UIElement>();
				if (childUI) {
					if (childUI->GetRectTransform()) {
						childUI->GetRectTransform()->SetDirty();
					}
					// Recurse into grandchildren
					childUI->PropagateDirtyToChildren();
				}
			}
		}

		void UIElement::OnAwake() {
			SyncRectTransform();
			// Initialize lastParent
			if (GetOwner()) {
				lastParent = GetOwner()->GetParent();
			}
		}

		void UIElement::OnUpdate(float deltaTime) {
			SyncRectTransform();

			// Detect parent changes and mark transform dirty
			if (GetOwner()) {
				ECS::GameObject* currentParent = GetOwner()->GetParent();
				if (currentParent != lastParent) {
					// Parent changed - mark transform as dirty so it recalculates
					if (rectTransform) {
						rectTransform->SetDirty();
					}
					lastParent = currentParent;
				}
			}
		}

	}
}
