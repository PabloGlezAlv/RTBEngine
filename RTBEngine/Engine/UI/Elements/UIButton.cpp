#include "UIButton.h"
#include "UIImage.h"
#include "UIPanel.h"
#include "../../ECS/GameObject.h"
#include "../../Core/Logger.h"

namespace RTBEngine {
	namespace UI {

		using ThisClass = UIButton;
		RTB_REGISTER_COMPONENT(UIButton)
			RTB_PROPERTY_COLOR(normalColor)
			RTB_PROPERTY_COLOR(hoveredColor)
			RTB_PROPERTY_COLOR(pressedColor)
			RTB_PROPERTY_COLOR(disabledColor)
			RTB_PROPERTY(interactable)
		RTB_END_REGISTER(UIButton)

		UIButton::UIButton()
			: targetImage(nullptr)
			, targetPanel(nullptr)
			, state(ButtonState::Normal)
			, onClick(nullptr)
		{
		}

		UIButton::~UIButton() {
		}

		void UIButton::OnAwake() {
		}

		void UIButton::ResolveTarget() {
			if (!owner) return;
			if (!targetImage) targetImage = owner->GetComponent<UIImage>();
			if (!targetPanel) targetPanel = owner->GetComponent<UIPanel>();
		}

		void UIButton::SetNormalColor(const Math::Vector4& color) {
			normalColor = color;
		}

		void UIButton::SetHoveredColor(const Math::Vector4& color) {
			hoveredColor = color;
		}

		void UIButton::SetPressedColor(const Math::Vector4& color) {
			pressedColor = color;
		}

		void UIButton::SetDisabledColor(const Math::Vector4& color) {
			disabledColor = color;
		}

		void UIButton::SetOnClick(std::function<void()> callback) {
			onClick = callback;
		}

		void UIButton::UpdateVisuals() {
			ResolveTarget();
			Math::Vector4 color = GetCurrentColor();
			if (targetImage) {
				targetImage->SetTint(color);
			} else if (targetPanel) {
				targetPanel->SetBackgroundColor(color);
			}
		}

		void UIButton::SetInteractable(bool value) {
			interactable = value;
			if (!interactable) {
				state = ButtonState::Disabled;
			}
			else if (state == ButtonState::Disabled) {
				state = ButtonState::Normal;
			}
			UpdateVisuals();
		}

		void UIButton::OnPointerEnter(const PointerEventData& eventData) {
			if (!interactable) return;
			if (state == ButtonState::Normal) {
				state = ButtonState::Hovered;
				UpdateVisuals();
			}
		}

		void UIButton::OnPointerExit(const PointerEventData& eventData) {
			if (!interactable) return;
			state = ButtonState::Normal;
			UpdateVisuals();
		}

		void UIButton::OnPointerDown(const PointerEventData& eventData) {
			if (!interactable) return;
			state = ButtonState::Pressed;
			UpdateVisuals();
		}

		void UIButton::OnPointerUp(const PointerEventData& eventData) {
			if (!interactable) return;
			state = ButtonState::Hovered;
			UpdateVisuals();
		}

		void UIButton::OnPointerClick(const PointerEventData& eventData) {
			if (!interactable) return;
			if (onClick) {
				onClick();
			}
		}

		Math::Vector4 UIButton::GetCurrentColor() const {
			switch (state) {
			case ButtonState::Hovered:
				return hoveredColor;
			case ButtonState::Pressed:
				return pressedColor;
			case ButtonState::Disabled:
				return disabledColor;
			case ButtonState::Normal:
			default:
				return normalColor;
			}
		}

	}
}
