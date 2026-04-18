#pragma once
#include "../../RTBEngineAPI.h"
#include "IEventSystemHandler.h"
#include "PointerEventData.h"

namespace RTBEngine {
	namespace UI {

		class RTB_API IBeginDragHandler : public virtual IEventSystemHandler {
		public:
			virtual void OnBeginDrag(const PointerEventData& eventData) = 0;
		};

	}
}
