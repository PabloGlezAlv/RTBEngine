#pragma once

#include "../../Scene/Component.h"
#include "../../Math/Vectors/Vector4.h"
#include "../../Reflection/PropertyMacros.h"
#include "../../RTBEngineAPI.h"
#include "../EventSystem/IPointerDownHandler.h"
#include <string>

namespace RTBEngine {
	namespace UI {
		class UIPanel;
		class UIText;

#pragma warning(push)
#pragma warning(disable: 4251)
		class RTB_API UIInputField : public ECS::Component,
		                             public IPointerDownHandler
		{
		public:
			UIInputField();
			virtual ~UIInputField();

			UIInputField(const UIInputField&) = delete;
			UIInputField& operator=(const UIInputField&) = delete;

			const std::string& GetText() const { return text; }
			void SetText(const std::string& value);

			void SetFocused(bool value);
			bool IsFocused() const { return isFocused; }

			void SetInteractable(bool value);
			bool IsInteractable() const { return interactable; }

			static UIInputField* GetFocusedField();
			static void ClearFocusedField();

			virtual void OnAwake() override;
			virtual void OnStart() override;
			virtual void OnUpdate(float deltaTime) override;
			virtual void OnDestroy() override;
			virtual void OnValidate() override;

			void OnPointerDown(const PointerEventData& eventData) override;

			std::string text;
			std::string placeholder = "Enter name";
			int maxLength = 24;
			bool interactable = true;

			Math::Vector4 textColor = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			Math::Vector4 placeholderColor = Math::Vector4(0.68f, 0.70f, 0.76f, 1.0f);
			Math::Vector4 normalColor = Math::Vector4(0.10f, 0.11f, 0.14f, 0.88f);
			Math::Vector4 focusedColor = Math::Vector4(0.16f, 0.18f, 0.25f, 0.96f);
			Math::Vector4 disabledColor = Math::Vector4(0.20f, 0.20f, 0.22f, 0.55f);

			UIText* textComponent = nullptr;
			UIPanel* backgroundPanel = nullptr;

			RTB_COMPONENT(UIInputField)

		private:
			bool isFocused = false;

			void ResolveReferences();
			void ClampTextToMaxLength();
			void AppendTextInput(const std::string& input);
			void RemoveLastCharacter();
			void UpdateVisuals();
		};
#pragma warning(pop)

	}
}
