#pragma once
#include "../../ECS/Component.h"
#include "../../Math/Vectors/Vector4.h"
#include "../EventSystem/IPointerEnterHandler.h"
#include "../EventSystem/IPointerExitHandler.h"
#include "../EventSystem/IPointerDownHandler.h"
#include "../EventSystem/IPointerUpHandler.h"
#include "../EventSystem/IPointerClickHandler.h"
#include "../../Reflection/PropertyMacros.h"
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

		class UIButton : public ECS::Component,
						 public IPointerEnterHandler,
						 public IPointerExitHandler,
						 public IPointerDownHandler,
						 public IPointerUpHandler,
						 public IPointerClickHandler
		{
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

			virtual void OnAwake() override;

			void OnPointerEnter(const PointerEventData& eventData) override;
			void OnPointerExit(const PointerEventData& eventData) override;
			void OnPointerDown(const PointerEventData& eventData) override;
			void OnPointerUp(const PointerEventData& eventData) override;
			void OnPointerClick(const PointerEventData& eventData) override;

			// Reflected properties
			Math::Vector4 normalColor = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			Math::Vector4 hoveredColor = Math::Vector4(0.9f, 0.9f, 0.9f, 1.0f);
			Math::Vector4 pressedColor = Math::Vector4(0.7f, 0.7f, 0.7f, 1.0f);
			Math::Vector4 disabledColor = Math::Vector4(0.5f, 0.5f, 0.5f, 0.5f);
			bool interactable = true;

			RTB_COMPONENT(UIButton)

		private:
			UIImage* targetImage = nullptr;
			UIPanel* targetPanel = nullptr;
			Math::Vector4 originalColor = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

			ButtonState state = ButtonState::Normal;

			std::function<void()> onClick;

			void UpdateVisuals();
			Math::Vector4 GetCurrentColor() const;
		};

	}
}
