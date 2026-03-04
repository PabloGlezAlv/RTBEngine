#pragma once
#include "../../RTBEngineAPI.h"
#include "../UIElement.h"
#include "../../Reflection/PropertyMacros.h"

namespace RTBEngine {
	namespace UI {

		class RTB_API UIContainer : public UIElement {
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
