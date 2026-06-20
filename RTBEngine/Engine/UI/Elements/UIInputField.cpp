#include "UIInputField.h"
#include "UIPanel.h"
#include "UIText.h"
#include "../../Scene/GameObject.h"
#include "../../Input/InputManager.h"
#include "../../Input/KeyCode.h"
#include <SDL.h>
#include <algorithm>

namespace RTBEngine {
	namespace UI {

		namespace {
			UIInputField* g_focusedField = nullptr;

			size_t CountUtf8Codepoints(const std::string& value) {
				size_t count = 0;
				for (unsigned char c : value) {
					if ((c & 0xC0) != 0x80) {
						++count;
					}
				}
				return count;
			}

			void TruncateUtf8(std::string& value, int maxLength) {
				if (maxLength <= 0) {
					return;
				}

				size_t count = 0;
				size_t byteIndex = 0;
				for (; byteIndex < value.size(); ++byteIndex) {
					unsigned char c = static_cast<unsigned char>(value[byteIndex]);
					if ((c & 0xC0) != 0x80) {
						if (count >= static_cast<size_t>(maxLength)) {
							break;
						}
						++count;
					}
				}

				if (byteIndex < value.size()) {
					value.erase(byteIndex);
				}
			}

			void RemoveLastUtf8Codepoint(std::string& value) {
				if (value.empty()) {
					return;
				}

				size_t index = value.size() - 1;
				while (index > 0) {
					unsigned char c = static_cast<unsigned char>(value[index]);
					if ((c & 0xC0) != 0x80) {
						break;
					}
					--index;
				}
				value.erase(index);
			}
		}

		using ThisClass = UIInputField;
		RTB_REGISTER_COMPONENT(UIInputField)
			RTB_PROPERTY(text)
			RTB_PROPERTY(placeholder)
			RTB_PROPERTY_RANGE(maxLength, 0, 128)
			RTB_PROPERTY(interactable)
			RTB_PROPERTY_COLOR(textColor)
			RTB_PROPERTY_COLOR(placeholderColor)
			RTB_PROPERTY_COLOR(normalColor)
			RTB_PROPERTY_COLOR(focusedColor)
			RTB_PROPERTY_COLOR(disabledColor)
			RTB_PROPERTY_COMPONENT(textComponent, UIText)
			RTB_PROPERTY_COMPONENT(backgroundPanel, UIPanel)
		RTB_END_REGISTER(UIInputField)

		UIInputField::UIInputField() {
		}

		UIInputField::~UIInputField() {
		}

		UIInputField* UIInputField::GetFocusedField() {
			return g_focusedField;
		}

		void UIInputField::ClearFocusedField() {
			if (g_focusedField) {
				g_focusedField->SetFocused(false);
			}
		}

		void UIInputField::OnAwake() {
			SetTimeMode(ECS::ComponentTimeMode::Unscaled);
			SetUpdateTickEnabled(false);
		}

		void UIInputField::OnStart() {
			ResolveReferences();
			ClampTextToMaxLength();
			UpdateVisuals();
		}

		void UIInputField::OnUpdate(float deltaTime) {
			(void)deltaTime;

			if (!isFocused || !interactable) {
				SetUpdateTickEnabled(false);
				return;
			}

			Input::InputManager& input = Input::InputManager::GetInstance();
			bool changed = false;

			const std::string& frameText = input.GetTextInput();
			if (!frameText.empty()) {
				AppendTextInput(frameText);
				changed = true;
			}

			if (input.IsKeyJustPressed(Input::KeyCode::Backspace)) {
				RemoveLastCharacter();
				changed = true;
			}

			if (input.IsKeyJustPressed(Input::KeyCode::Enter) ||
				input.IsKeyJustPressed(Input::KeyCode::NumpadEnter) ||
				input.IsKeyJustPressed(Input::KeyCode::Escape)) {
				SetFocused(false);
				return;
			}

			if (changed) {
				UpdateVisuals();
			}
		}

		void UIInputField::OnDestroy() {
			if (g_focusedField == this) {
				g_focusedField = nullptr;
				SDL_StopTextInput();
			}
			isFocused = false;
			textComponent = nullptr;
			backgroundPanel = nullptr;
			SetUpdateTickEnabled(false);
		}

		void UIInputField::OnValidate() {
			maxLength = std::max(0, maxLength);
			ResolveReferences();
			ClampTextToMaxLength();
			if (!interactable && isFocused) {
				SetFocused(false);
			}
			UpdateVisuals();
		}

		void UIInputField::OnPointerDown(const PointerEventData& eventData) {
			(void)eventData;
			if (interactable) {
				SetFocused(true);
			}
		}

		void UIInputField::SetText(const std::string& value) {
			text = value;
			ClampTextToMaxLength();
			UpdateVisuals();
		}

		void UIInputField::SetFocused(bool value) {
			value = value && interactable;

			if (value && g_focusedField != this) {
				if (g_focusedField) {
					g_focusedField->SetFocused(false);
				}
				g_focusedField = this;
			}

			if (!value && g_focusedField == this) {
				g_focusedField = nullptr;
			}

			if (isFocused == value) {
				return;
			}

			isFocused = value;
			if (isFocused) {
				SDL_StartTextInput();
				SetUpdateTickEnabled(true);
			} else {
				if (!g_focusedField) {
					SDL_StopTextInput();
				}
				SetUpdateTickEnabled(false);
			}

			UpdateVisuals();
		}

		void UIInputField::SetInteractable(bool value) {
			interactable = value;
			if (!interactable) {
				SetFocused(false);
			}
			UpdateVisuals();
		}

		void UIInputField::ResolveReferences() {
			if (!owner) {
				return;
			}

			if (!backgroundPanel) {
				backgroundPanel = owner->GetComponent<UIPanel>();
			}

			if (!textComponent) {
				textComponent = owner->GetComponentInChildren<UIText>();
			}
		}

		void UIInputField::ClampTextToMaxLength() {
			TruncateUtf8(text, maxLength);
		}

		void UIInputField::AppendTextInput(const std::string& input) {
			if (input.empty()) {
				return;
			}

			if (maxLength <= 0) {
				text += input;
				return;
			}

			const size_t remaining = static_cast<size_t>(maxLength) - std::min(
				static_cast<size_t>(maxLength),
				CountUtf8Codepoints(text));
			if (remaining == 0) {
				return;
			}

			std::string accepted = input;
			TruncateUtf8(accepted, static_cast<int>(remaining));
			text += accepted;
		}

		void UIInputField::RemoveLastCharacter() {
			RemoveLastUtf8Codepoint(text);
		}

		void UIInputField::UpdateVisuals() {
			ResolveReferences();

			if (backgroundPanel) {
				backgroundPanel->SetBackgroundColor(
					!interactable ? disabledColor : (isFocused ? focusedColor : normalColor));
			}

			if (textComponent) {
				const bool showingPlaceholder = text.empty();
				textComponent->SetText(showingPlaceholder ? placeholder : text);
				textComponent->SetColor(showingPlaceholder ? placeholderColor : textColor);
			}
		}

	}
}
