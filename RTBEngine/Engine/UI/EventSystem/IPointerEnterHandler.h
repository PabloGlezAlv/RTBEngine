#pragma once
#include "../../RTBEngineAPI.h"
#include "IEventSystemHandler.h"
#include "PointerEventData.h"

namespace RTBEngine {
	namespace UI {

		class RTB_API IPointerEnterHandler : public virtual IEventSystemHandler {
		public:
			virtual void OnPointerEnter(const PointerEventData& eventData) = 0;
		};

	}
}
