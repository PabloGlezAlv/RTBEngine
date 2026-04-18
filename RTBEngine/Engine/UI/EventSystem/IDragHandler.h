#pragma once
#include "../../RTBEngineAPI.h"
#include "IEventSystemHandler.h"
#include "PointerEventData.h"

namespace RTBEngine {
	namespace UI {

		class RTB_API IDragHandler : public virtual IEventSystemHandler {
		public:
			virtual void OnDrag(const PointerEventData& eventData) = 0;
		};

	}
}
