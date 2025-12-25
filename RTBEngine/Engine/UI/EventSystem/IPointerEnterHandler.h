#pragma once
#include "IEventSystemHandler.h"
#include "PointerEventData.h"

namespace RTBEngine {
	namespace UI {

		class IPointerEnterHandler : public virtual IEventSystemHandler {
		public:
			virtual void OnPointerEnter(const PointerEventData& eventData) = 0;
		};

	}
}
