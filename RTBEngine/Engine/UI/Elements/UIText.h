#pragma once
#include "../UIElement.h"
#include "../../Math/Vectors/Vector4.h"
#include <string>
#include "../../Reflection/PropertyMacros.h"

namespace RTBEngine {
	namespace Rendering {
		class Font;
	}
}

namespace RTBEngine {
	namespace UI {

		enum class TextAlignment {
			Left,
			Center,
			Right
		};

		class UIText : public UIElement {
		public:
			UIText();
			virtual ~UIText();

			void SetText(const std::string& text);
			std::string GetText() const { return text; }

			void SetColor(const Math::Vector4& color);
			Math::Vector4 GetColor() const { return color; }

			void SetFontSize(float size);
			float GetFontSize() const { return fontSize; }

			void SetFont(Rendering::Font* font);
			Rendering::Font* GetFont() const { return font; }

			void SetAlignment(TextAlignment align);
			TextAlignment GetAlignment() const { return alignment; }

			virtual void Render() override;

			// Reflected properties
			std::string text = "Text";
			Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			float fontSize = 16.0f;
			TextAlignment alignment = TextAlignment::Left;
			Rendering::Font* font = nullptr;

			RTB_COMPONENT(UIText)
		};

	}
}
