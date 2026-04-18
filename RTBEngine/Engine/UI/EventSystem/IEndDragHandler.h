#pragma once
#include "../../RTBEngineAPI.h"
#include "IEventSystemHandler.h"
#include "PointerEventData.h"

namespace RTBEngine {
	namespace UI {

		class RTB_API IEndDragHandler : public virtual IEventSystemHandler {
		public:
			virtual void OnEndDrag(const PointerEventData& eventData) = 0;
		};

	}
}
