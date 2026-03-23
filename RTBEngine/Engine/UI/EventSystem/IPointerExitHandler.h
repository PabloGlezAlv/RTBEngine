#pragma once
#include "../../RTBEngineAPI.h"
#include "IEventSystemHandler.h"
#include "PointerEventData.h"

namespace RTBEngine {
	namespace UI {

		class RTB_API IPointerExitHandler : public virtual IEventSystemHandler {
		public:
			virtual void OnPointerExit(const PointerEventData& eventData) = 0;
		};

	}
}
