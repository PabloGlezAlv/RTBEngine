#include "UIText.h"
#include "../../Rendering/Font.h"
#include "../../Core/ResourceManager.h"
#include "../UIRenderContext.h"
#include <imgui.h>

namespace RTBEngine {
	namespace UI {

		using ThisClass = UIText;
		RTB_REGISTER_COMPONENT(UIText)
			RTB_PROPERTY(text)
			RTB_PROPERTY_COLOR(color)
			RTB_PROPERTY(fontSize)
			RTB_PROPERTY_ENUM(alignment, "Left", "Center", "Right")
			RTB_PROPERTY_FONT(font)
			RTB_PROPERTY(isVisible)
		RTB_END_REGISTER(UIText)

		UIText::UIText()
		{
		}

		UIText::~UIText() {
		}

		void UIText::SetText(const std::string& newText) {
			text = newText;
		}

		void UIText::SetColor(const Math::Vector4& newColor) {
			color = newColor;
		}

		void UIText::SetFontSize(float size) {
			fontSize = size;
		}

		void UIText::SetAlignment(TextAlignment align) {
			alignment = align;
		}

		void UIText::SetFont(Rendering::Font* newFont) {
			font = newFont;
		}

		void UIText::Render() {
			if (!isVisible || text.empty()) return;

			Math::Vector4 screenRect = rectTransform->GetScreenRect();
			ImDrawList* drawList = UIRenderContext::GetDrawList();
			Math::Vector2 offset = UIRenderContext::Offset;

			Rendering::Font* activeFont = font;
			if (!activeFont) {
				activeFont = Core::ResourceManager::GetInstance().GetDefaultFont();
			}

			ImFont* imFont = nullptr;
			if (activeFont) {
				imFont = activeFont->GetImFont(fontSize);
			}

			if (!imFont) {
				imFont = ImGui::GetFont();
			}

			ImVec2 textSize = imFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());

			ImVec2 textPos(screenRect.x + offset.x, screenRect.y + offset.y);

			switch (alignment) {
			case TextAlignment::Center:
				textPos.x += (screenRect.z - textSize.x) * 0.5f;
				textPos.y += (screenRect.w - textSize.y) * 0.5f;
				break;
			case TextAlignment::Right:
				textPos.x += screenRect.z - textSize.x;
				textPos.y += (screenRect.w - textSize.y) * 0.5f;
				break;
			case TextAlignment::Left:
			default:
				textPos.y += (screenRect.w - textSize.y) * 0.5f;
				break;
			}

			ImU32 textColor = IM_COL32(
				static_cast<int>(color.x * 255),
				static_cast<int>(color.y * 255),
				static_cast<int>(color.z * 255),
				static_cast<int>(color.w * 255)
			);

			drawList->AddText(imFont, fontSize, textPos, textColor, text.c_str());
		}

	}
}
