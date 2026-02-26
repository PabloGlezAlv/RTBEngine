#include "UIContainer.h"

namespace RTBEngine {
	namespace UI {

		using ThisClass = UIContainer;
		RTB_REGISTER_COMPONENT(UIContainer)
			{ using ThisClass = UIElement; RTB_PROPERTY(isVisible) }
			{ using ThisClass = UIElement; RTB_PROPERTY(anchorMin) }
			{ using ThisClass = UIElement; RTB_PROPERTY(anchorMax) }
			{ using ThisClass = UIElement; RTB_PROPERTY(anchoredPosition) }
			{ using ThisClass = UIElement; RTB_PROPERTY(sizeDelta) }
		RTB_END_REGISTER(UIContainer)

		UIContainer::UIContainer() {
		}

		UIContainer::~UIContainer() {
		}

	}
}
