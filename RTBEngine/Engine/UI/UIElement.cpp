#include "UIElement.h"

namespace RTBEngine {
	namespace UI {

		UIElement::UIElement() {
			rectTransform = std::make_unique<RectTransform>();
		}

		UIElement::~UIElement() {

		}

		void UIElement::OnAwake() {
			if (rectTransform) {
				rectTransform->SetAnchorMin(Math::Vector2(0.5f, 0.5f));
				rectTransform->SetAnchorMax(Math::Vector2(0.5f, 0.5f));
				rectTransform->SetPivot(Math::Vector2(0.5f, 0.5f));
				rectTransform->SetSize(Math::Vector2(100.0f, 100.0f));
			}
		}

		void UIElement::OnUpdate(float deltaTime) {
			
		}

	}
}
