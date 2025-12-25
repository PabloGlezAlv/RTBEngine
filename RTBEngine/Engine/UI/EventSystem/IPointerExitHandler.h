#pragma once
#include "IEventSystemHandler.h"
#include "PointerEventData.h"

namespace RTBEngine {
	namespace UI {

		class IPointerExitHandler : public virtual IEventSystemHandler {
		public:
			virtual void OnPointerExit(const PointerEventData& eventData) = 0;
		};

	}
}
