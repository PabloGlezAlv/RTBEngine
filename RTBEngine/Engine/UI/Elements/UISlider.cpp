#include "UISlider.h"
#include "UIPanel.h"
#include "../../ECS/GameObject.h"
#include "../../Math/Vectors/Vector2.h"
#include <algorithm>
#include <cmath>

namespace RTBEngine {
	namespace UI {

		using ThisClass = UISlider;
		RTB_REGISTER_COMPONENT(UISlider)
			RTB_PROPERTY(minValue)
			RTB_PROPERTY(maxValue)
			RTB_PROPERTY(value)
			RTB_PROPERTY(interactable)
			RTB_PROPERTY_COMPONENT(fillPanel, UIPanel)
			RTB_PROPERTY_COMPONENT(handlePanel, UIPanel)
		RTB_END_REGISTER(UISlider)

		UISlider::UISlider()
			: targetPanel(nullptr)
			, isDragging(false)
		{
		}

		UISlider::~UISlider() {
		}

		void UISlider::SetMinValue(float min) {
			minValue = min;
			ClampValueRange();
			UpdateVisuals();
		}

		void UISlider::SetMaxValue(float max) {
			maxValue = max;
			ClampValueRange();
			UpdateVisuals();
		}

		void UISlider::SetValue(float newValue) {
			value = newValue;
			ClampValueRange();
			UpdateVisuals();
		}

		void UISlider::SetNormalizedValue(float normalizedValue) {
			const float clamped = std::clamp(normalizedValue, 0.0f, 1.0f);
			if (maxValue <= minValue) {
				value = minValue;
			} else {
				value = minValue + (maxValue - minValue) * clamped;
			}

			ClampValueRange();
			UpdateVisuals();
		}

		float UISlider::GetNormalizedValue() const {
			if (maxValue <= minValue) {
				return 0.0f;
			}

			return std::clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f);
		}

		void UISlider::SetInteractable(bool enabled) {
			interactable = enabled;
			if (!interactable) {
				isDragging = false;
			}
		}

		void UISlider::OnStart() {
			ResolveTrackPanel();
			ClampValueRange();
			UpdateVisuals();
		}

		void UISlider::OnValidate() {
			ResolveTrackPanel();
			ClampValueRange();
			UpdateVisuals();
		}

		void UISlider::OnPointerDown(const PointerEventData& eventData) {
			if (!interactable) return;
			SetValueFromScreenPosition(eventData.position.x);
		}

		void UISlider::OnPointerUp(const PointerEventData& eventData) {
			(void)eventData;
			isDragging = false;
		}

		void UISlider::OnBeginDrag(const PointerEventData& eventData) {
			if (!interactable) return;
			isDragging = true;
			SetValueFromScreenPosition(eventData.position.x);
		}

		void UISlider::OnDrag(const PointerEventData& eventData) {
			if (!interactable || !isDragging) return;
			SetValueFromScreenPosition(eventData.position.x);
		}

		void UISlider::OnEndDrag(const PointerEventData& eventData) {
			(void)eventData;
			isDragging = false;
		}

		void UISlider::ResolveTrackPanel() {
			if (!owner) return;
			if (!targetPanel) {
				targetPanel = owner->GetComponent<UIPanel>();
			}
		}

		void UISlider::ClampValueRange() {
			if (maxValue < minValue) {
				maxValue = minValue;
			}

			value = std::clamp(value, minValue, maxValue);
		}

		void UISlider::UpdateVisuals() {
			ResolveTrackPanel();

			const float normalizedValue = GetNormalizedValue();
			const float trackWidth = GetTrackWidth();

			if (fillPanel) {
				fillPanel->SetRaycastTarget(false);
				const Math::Vector2 currentSize = fillPanel->GetSizeDelta();
				fillPanel->SetSizeDelta(Math::Vector2(trackWidth * normalizedValue, currentSize.y));
			}

			if (handlePanel) {
				handlePanel->SetRaycastTarget(false);
				const Math::Vector2 currentPosition = handlePanel->GetAnchoredPosition();
				handlePanel->SetAnchoredPosition(Math::Vector2(trackWidth * normalizedValue, currentPosition.y));
			}
		}

		void UISlider::SetValueFromScreenPosition(float screenX) {
			ResolveTrackPanel();
			if (!targetPanel) {
				return;
			}

			const Math::Vector4 worldRect = targetPanel->GetRectTransform()->GetWorldRect();
			if (worldRect.z <= 0.0f) {
				SetNormalizedValue(0.0f);
				return;
			}

			const float normalized = (screenX - worldRect.x) / worldRect.z;
			SetNormalizedValue(normalized);
		}

		float UISlider::GetTrackWidth() const {
			if (!targetPanel) {
				return 0.0f;
			}

			return std::max(0.0f, targetPanel->GetSizeDelta().x);
		}

	}
}
