#include "UIText.h"
#include "../../Rendering/Font.h"
#include "../../Core/ResourceManager.h"
#include "../UIRenderContext.h"
#include "../../Core/Logger.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace RTBEngine {
	namespace UI {

		using ThisClass = UIText;
		RTB_REGISTER_COMPONENT(UIText)
			RTB_PROPERTY(text)
			RTB_PROPERTY_COLOR(color)
			RTB_PROPERTY(fontSize)
			RTB_PROPERTY_ENUM(alignment, "Left", "Center", "Right")
			RTB_PROPERTY_FONT(font)
			{ using ThisClass = UIElement; RTB_PROPERTY(isVisible) }
			{ using ThisClass = UIElement; RTB_PROPERTY(raycastTarget) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(anchorMin) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(anchorMax) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(pivot) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(anchoredPosition) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(sizeDelta) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(rotation) }
			{ using ThisClass = UIElement; RTB_PROPERTY_SERIALIZED_HIDDEN(scale) }
		RTB_END_REGISTER(UIText)

		UIText::UIText()
		{
			raycastTarget = false;
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

			Math::Vector4 screenRect = rectTransform->GetWorldRect();
			ImDrawList* drawList = UIRenderContext::GetDrawList();
			Math::Vector2 offset = UIRenderContext::Offset;
			Math::Vector2 lossyScale = rectTransform->GetLossyScale();
			float effectiveScale = (std::abs(lossyScale.x) + std::abs(lossyScale.y)) * 0.5f;
			if (effectiveScale < 0.01f) {
				effectiveScale = 0.01f;
			}
			float effectiveFontSize = fontSize * effectiveScale;
			if (effectiveFontSize < 1.0f) {
				effectiveFontSize = 1.0f;
			}

			Rendering::Font* activeFont = font;
			if (!activeFont) {
				activeFont = Core::ResourceManager::GetInstance().GetDefaultFont();
			}

			ImFont* imFont = nullptr;
			if (activeFont) {
				imFont = activeFont->GetImFont(effectiveFontSize);
			}

			if (!imFont) {
				imFont = ImGui::GetFont();
			}

			ImVec2 textSize = imFont->CalcTextSizeA(effectiveFontSize, FLT_MAX, 0.0f, text.c_str());

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

			drawList->AddText(imFont, effectiveFontSize, textPos, textColor, text.c_str());
		}

	}
}
