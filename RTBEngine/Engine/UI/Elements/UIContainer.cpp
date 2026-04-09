#include "UIContainer.h"

namespace RTBEngine {
	namespace UI {

		using ThisClass = UIContainer;
		RTB_REGISTER_COMPONENT(UIContainer)
			{ using ThisClass = UIElement; RTB_PROPERTY(isVisible) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(anchorMin) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(anchorMax) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(pivot) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(anchoredPosition) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(sizeDelta) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(rotation) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(scale) }
		RTB_END_REGISTER(UIContainer)

		UIContainer::UIContainer() {
		}

		UIContainer::~UIContainer() {
		}

	}
}
