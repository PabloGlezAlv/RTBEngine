#pragma once
#include "../UIElement.h"
#include "../../Reflection/PropertyMacros.h"

namespace RTBEngine {
	namespace UI {

		class UIContainer : public UIElement {
		public:
			UIContainer();
			virtual ~UIContainer();

			UIContainer(const UIContainer&) = delete;
			UIContainer& operator=(const UIContainer&) = delete;

			virtual void Render() override {}

			RTB_COMPONENT(UIContainer)
		};

	}
}
