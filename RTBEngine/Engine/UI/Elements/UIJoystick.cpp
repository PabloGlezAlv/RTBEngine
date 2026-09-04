#include "UIJoystick.h"

#include "UIImage.h"
#include "../../Scene/GameObject.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace RTBEngine {
	namespace UI {

		using ThisClass = UIJoystick;
		RTB_REGISTER_COMPONENT(UIJoystick)
			RTB_PROPERTY_COMPONENT(handleImage, UIImage)
			RTB_PROPERTY_RANGE(deadZone, 0.0f, 1.0f)
			RTB_PROPERTY_RANGE(maxDistance, 0.0f, 512.0f)
			RTB_PROPERTY(interactable)
		RTB_END_REGISTER(UIJoystick)

		UIJoystick::UIJoystick()
		{
		}

		UIJoystick::~UIJoystick()
		{
		}

		void UIJoystick::OnStart()
		{
			ResolveImages();
			ResetHandle();
		}

		void UIJoystick::OnValidate()
		{
			deadZone = std::clamp(deadZone, 0.0f, 1.0f);
			maxDistance = std::max(0.0f, maxDistance);
			ResolveImages();
			if (!isDragging) {
				ResetHandle();
			}
		}

		void UIJoystick::OnPointerDown(const PointerEventData& eventData)
		{
			if (!interactable) return;

			isDragging = true;
			UpdateValueFromScreenPosition(eventData.position);
		}

		void UIJoystick::OnBeginDrag(const PointerEventData& eventData)
		{
			if (!interactable) return;

			isDragging = true;
			UpdateValueFromScreenPosition(eventData.position);
		}

		void UIJoystick::OnDrag(const PointerEventData& eventData)
		{
			if (!interactable || !isDragging) return;
			UpdateValueFromScreenPosition(eventData.position);
		}

		void UIJoystick::OnEndDrag(const PointerEventData& eventData)
		{
			if (!interactable) return;

			UpdateValueFromScreenPosition(eventData.position);

			const Math::Vector2 finalValue =
				value.LengthSquared() > 0.0f ? value : Math::Vector2::Zero();

			isDragging = false;
			value = Math::Vector2::Zero();
			ResetHandle();

			if (finalValue.LengthSquared() > 0.0f) {
				releasedEvent.Invoke(finalValue);
			}
		}

		Core::EventSubscription UIJoystick::SubscribeToReleased(ReleasedCallback callback)
		{
			return releasedEvent.Subscribe(std::move(callback));
		}

		void UIJoystick::ResolveImages()
		{
			backgroundImage = owner->GetComponent<UIImage>();

			if (handleImage && !hasHandleCenterPosition) {
				handleCenterPosition = handleImage->GetAnchoredPosition();
				hasHandleCenterPosition = true;
			}
		}

		void UIJoystick::ResetHandle()
		{
			if (!handleImage) return;

			if (!hasHandleCenterPosition) {
				handleCenterPosition = handleImage->GetAnchoredPosition();
				hasHandleCenterPosition = true;
			}

			handleImage->SetAnchoredPosition(handleCenterPosition);
		}

		void UIJoystick::UpdateValueFromScreenPosition(const Math::Vector2& screenPosition)
		{
			ResolveImages();
			if (!backgroundImage) {
				value = Math::Vector2::Zero();
				ResetHandle();
				return;
			}

			const Math::Vector4 rect = backgroundImage->GetRectTransform()->GetWorldRect();
			const float radius = GetEffectiveRadius();
			if (radius <= 0.0f) {
				value = Math::Vector2::Zero();
				ResetHandle();
				return;
			}

			const Math::Vector2 center(rect.x + rect.z * 0.5f, rect.y + rect.w * 0.5f);
			Math::Vector2 raw(
				(screenPosition.x - center.x) / radius,
				(center.y - screenPosition.y) / radius);

			const float rawLength = raw.Length();
			if (rawLength > 1.0f) {
				raw /= rawLength;
			}

			if (raw.Length() < deadZone) {
				value = Math::Vector2::Zero();
			} else {
				value = raw;
			}

			if (handleImage) {
				if (!hasHandleCenterPosition) {
					handleCenterPosition = handleImage->GetAnchoredPosition();
					hasHandleCenterPosition = true;
				}

				handleImage->SetAnchoredPosition(
					handleCenterPosition + Math::Vector2(raw.x * radius, raw.y * radius));
			}
		}

		float UIJoystick::GetEffectiveRadius() const
		{
			if (maxDistance > 0.0f) {
				return maxDistance;
			}

			if (!backgroundImage) {
				return 0.0f;
			}

			const Math::Vector4 rect = backgroundImage->GetRectTransform()->GetWorldRect();
			return std::max(0.0f, std::min(rect.z, rect.w) * 0.5f);
		}

	}
}
