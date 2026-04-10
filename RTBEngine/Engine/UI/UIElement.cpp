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

			PropagateDirtyToChildren();
		}

		void UIElement::PropagateDirtyToChildren() {
			if (!GetOwner()) return;

			for (ECS::GameObject* child : GetOwner()->GetChildren()) {
				if (!child) continue;

				UIElement* childUI = child->GetComponent<UIElement>();
				if (childUI) {
					if (childUI->GetRectTransform()) {
						childUI->GetRectTransform()->SetDirty();
					}
					childUI->PropagateDirtyToChildren();
				}
			}
		}

		void UIElement::SetAnchorMin(const Math::Vector2& value) {
			anchorMin = value;
			if (rectTransform) { rectTransform->SetAnchorMin(value); }
			PropagateDirtyToChildren();
		}

		void UIElement::SetAnchorMax(const Math::Vector2& value) {
			anchorMax = value;
			if (rectTransform) { rectTransform->SetAnchorMax(value); }
			PropagateDirtyToChildren();
		}

		void UIElement::SetPivot(const Math::Vector2& value) {
			pivot = value;
			if (rectTransform) { rectTransform->SetPivot(value); }
			PropagateDirtyToChildren();
		}

		void UIElement::SetAnchoredPosition(const Math::Vector2& value) {
			anchoredPosition = value;
			if (rectTransform) { rectTransform->SetAnchoredPosition(value); }
			PropagateDirtyToChildren();
		}

		void UIElement::SetSizeDelta(const Math::Vector2& value) {
			sizeDelta = value;
			if (rectTransform) { rectTransform->SetSize(value); }
			PropagateDirtyToChildren();
		}

		void UIElement::SetRotation(float degrees) {
			rotation = degrees;
			if (rectTransform) { rectTransform->SetRotation(degrees); }
			PropagateDirtyToChildren();
		}

		void UIElement::SetScale(const Math::Vector2& value) {
			scale = value;
			if (rectTransform) { rectTransform->SetScale(value); }
			PropagateDirtyToChildren();
		}

		void UIElement::OnAwake() {
			SyncRectTransform();
		}

		void UIElement::OnParentChanged(ECS::GameObject* oldParent, ECS::GameObject* newParent) {
			(void)oldParent;
			(void)newParent;

			if (rectTransform) {
				rectTransform->SetDirty();
			}
			PropagateDirtyToChildren();
		}

	}
}
