#include "UIButton.h"
#include "UIImage.h"
#include "UIPanel.h"
#include "../../ECS/GameObject.h"
#include <iostream>

namespace RTBEngine {
	namespace UI {

		UIButton::UIButton()
			: targetImage(nullptr)
			, targetPanel(nullptr)
			, state(ButtonState::Normal)
			, interactable(true)
			, normalColor(1.0f, 1.0f, 1.0f, 1.0f)
			, hoveredColor(0.9f, 0.9f, 0.9f, 1.0f)
			, pressedColor(0.7f, 0.7f, 0.7f, 1.0f)
			, disabledColor(0.5f, 0.5f, 0.5f, 0.5f)
			, originalColor(1.0f, 1.0f, 1.0f, 1.0f)
			, onClick(nullptr)
		{
		}

		UIButton::~UIButton() {
		}

		void UIButton::OnAwake() {
			if (owner) {
				targetImage = owner->GetComponent<UIImage>();
				targetPanel = owner->GetComponent<UIPanel>();
				
				if (targetImage) {
					originalColor = targetImage->GetTint();
				} else if (targetPanel) {
					originalColor = targetPanel->GetBackgroundColor();
				}
			}
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
			Math::Vector4 currentColor = GetCurrentColor();
			Math::Vector4 finalColor(
				originalColor.x * currentColor.x,
				originalColor.y * currentColor.y,
				originalColor.z * currentColor.z,
				originalColor.w * currentColor.w
			);

			if (targetImage) {
				targetImage->SetTint(finalColor);
			} else if (targetPanel) {
				targetPanel->SetBackgroundColor(finalColor);
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

		void UIButton::OnPointerEnter() {
			std::cout << "[UIButton] OnPointerEnter" << std::endl;
			if (!interactable) return;
			if (state == ButtonState::Normal) {
				state = ButtonState::Hovered;
				UpdateVisuals();
			}
		}

		void UIButton::OnPointerExit() {
			std::cout << "[UIButton] OnPointerExit" << std::endl;
			if (!interactable) return;
			state = ButtonState::Normal;
			UpdateVisuals();
		}

		void UIButton::OnPointerDown() {
			std::cout << "[UIButton] OnPointerDown" << std::endl;
			if (!interactable) return;
			state = ButtonState::Pressed;
			UpdateVisuals();
		}

		void UIButton::OnPointerUp() {
			std::cout << "[UIButton] OnPointerUp" << std::endl;
			if (!interactable) return;

			if (state == ButtonState::Pressed) {
				if (onClick) {
					onClick();
				}
			}

			state = ButtonState::Hovered;
			UpdateVisuals();
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

		void UIButton::Render() {
		}

	}
}
