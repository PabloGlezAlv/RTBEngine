#pragma once
#include "../UIElement.h"
#include "../../Math/Vectors/Vector4.h"
#include <functional>

namespace RTBEngine {
	namespace UI {
		class UIImage;
		class UIPanel;

		enum class ButtonState {
			Normal,
			Hovered,
			Pressed,
			Disabled
		};

		class UIButton : public UIElement {
		public:
			UIButton();
			virtual ~UIButton();

			void SetNormalColor(const Math::Vector4& color);
			void SetHoveredColor(const Math::Vector4& color);
			void SetPressedColor(const Math::Vector4& color);
			void SetDisabledColor(const Math::Vector4& color);

			void SetOnClick(std::function<void()> callback);
			void SetInteractable(bool interactable);
			bool IsInteractable() const { return interactable; }

			ButtonState GetState() const { return state; }

			virtual const char* GetTypeName() const override { return "UIButton"; }
			virtual void OnAwake() override;
			virtual void Render() override;

			void OnPointerEnter();
			void OnPointerExit();
			void OnPointerDown();
			void OnPointerUp();

		private:
			Math::Vector4 normalColor;
			Math::Vector4 hoveredColor;
			Math::Vector4 pressedColor;
			Math::Vector4 disabledColor;

			UIImage* targetImage;
			UIPanel* targetPanel;
			Math::Vector4 originalColor;

			ButtonState state;
			bool interactable;

			std::function<void()> onClick;

			void UpdateVisuals();
			Math::Vector4 GetCurrentColor() const;
		};

	}
}
