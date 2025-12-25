#pragma once
#include "IEventSystemHandler.h"
#include "PointerEventData.h"

namespace RTBEngine {
	namespace UI {

		class IPointerClickHandler : public virtual IEventSystemHandler {
		public:
			virtual void OnPointerClick(const PointerEventData& eventData) = 0;
		};

	}
}
