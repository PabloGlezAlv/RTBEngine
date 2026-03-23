#pragma once
#include "../../RTBEngineAPI.h"
#include "IEventSystemHandler.h"
#include "PointerEventData.h"

namespace RTBEngine {
	namespace UI {

		class RTB_API IPointerDownHandler : public virtual IEventSystemHandler {
		public:
			virtual void OnPointerDown(const PointerEventData& eventData) = 0;
		};

	}
}
