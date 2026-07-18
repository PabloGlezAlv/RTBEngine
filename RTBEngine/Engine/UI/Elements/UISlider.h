#pragma once
#include "../../Scene/Component.h"
#include "../../Reflection/PropertyMacros.h"
#include "../EventSystem/IPointerDownHandler.h"
#include "../EventSystem/IPointerUpHandler.h"
#include "../EventSystem/IBeginDragHandler.h"
#include "../EventSystem/IDragHandler.h"
#include "../EventSystem/IEndDragHandler.h"
#include "../../RTBEngineAPI.h"

namespace RTBEngine {
	namespace UI {
		class UIPanel;

#pragma warning(push)
#pragma warning(disable: 4251)
		class RTB_API UISlider : public Scene::Component,
		                         public IPointerDownHandler,
		                         public IPointerUpHandler,
		                         public IBeginDragHandler,
		                         public IDragHandler,
		                         public IEndDragHandler
		{
		public:
			UISlider();
			virtual ~UISlider();

			UISlider(const UISlider&) = delete;
			UISlider& operator=(const UISlider&) = delete;

			void SetMinValue(float min);
			void SetMaxValue(float max);
			void SetValue(float newValue);
			float GetValue() const { return value; }

			void SetNormalizedValue(float normalizedValue);
			float GetNormalizedValue() const;

			void SetInteractable(bool enabled);
			bool IsInteractable() const { return interactable; }

			virtual void OnStart() override;
			virtual void OnValidate() override;

			void OnPointerDown(const PointerEventData& eventData) override;
			void OnPointerUp(const PointerEventData& eventData) override;
			void OnBeginDrag(const PointerEventData& eventData) override;
			void OnDrag(const PointerEventData& eventData) override;
			void OnEndDrag(const PointerEventData& eventData) override;

			float minValue = 0.0f;
			float maxValue = 1.0f;
			float value = 0.5f;
			bool interactable = true;
			UIPanel* fillPanel = nullptr;
			UIPanel* handlePanel = nullptr;

			RTB_COMPONENT(UISlider)

		private:
			UIPanel* targetPanel = nullptr;
			bool isDragging = false;

			void ResolveTrackPanel();
			void ClampValueRange();
			void UpdateVisuals();
			void SetValueFromScreenPosition(float screenX);
			float GetTrackWidth() const;
		};
#pragma warning(pop)

	}
}
