#pragma once
#include "IEventSystemHandler.h"
#include "PointerEventData.h"

namespace RTBEngine {
	namespace UI {

		class IPointerUpHandler : public virtual IEventSystemHandler {
		public:
			virtual void OnPointerUp(const PointerEventData& eventData) = 0;
		};

	}
}
