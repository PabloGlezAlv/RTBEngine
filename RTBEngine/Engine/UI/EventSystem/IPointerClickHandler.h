#pragma once
#include "../../RTBEngineAPI.h"
#include "IEventSystemHandler.h"
#include "PointerEventData.h"

namespace RTBEngine {
	namespace UI {

		class RTB_API IPointerClickHandler : public virtual IEventSystemHandler {
		public:
			virtual void OnPointerClick(const PointerEventData& eventData) = 0;
		};

	}
}
