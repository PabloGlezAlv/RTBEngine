#include "UIElement.h"

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
		}

		void UIElement::OnAwake() {
			SyncRectTransform();
		}

		void UIElement::OnUpdate(float deltaTime) {
			SyncRectTransform();
		}

	}
}
