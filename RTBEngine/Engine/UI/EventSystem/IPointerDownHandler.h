#pragma once
#include "IEventSystemHandler.h"
#include "PointerEventData.h"

namespace RTBEngine {
	namespace UI {

		class IPointerDownHandler : public virtual IEventSystemHandler {
		public:
			virtual void OnPointerDown(const PointerEventData& eventData) = 0;
		};

	}
}
