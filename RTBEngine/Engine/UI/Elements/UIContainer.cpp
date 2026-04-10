#include "UIContainer.h"

namespace RTBEngine {
	namespace UI {

		using ThisClass = UIContainer;
		RTB_REGISTER_COMPONENT(UIContainer)
			{ using ThisClass = UIElement; RTB_PROPERTY(isVisible) }
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchorMin)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchorMax)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, pivot)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchoredPosition)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, sizeDelta)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, rotation)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, scale)
		RTB_END_REGISTER(UIContainer)

		UIContainer::UIContainer() {
		}

		UIContainer::~UIContainer() {
		}

	}
}
