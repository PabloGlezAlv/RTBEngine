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
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchorMin)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchorMax)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, pivot)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchoredPosition)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, sizeDelta)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, rotation)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, scale)
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

			Math::Vector4 screenRect = rectTransform.GetWorldRect();
			ImDrawList* drawList = UIRenderContext::GetDrawList();
			Math::Vector2 lossyScale = rectTransform.GetLossyScale();
			const float uiScale = UIRenderContext::IsValid ? UIRenderContext::UniformScale() : 1.0f;
			float effectiveScale = std::abs(lossyScale.y);
			if (effectiveScale < 0.01f) {
				effectiveScale = 0.01f;
			}
			float effectiveFontSize = fontSize * effectiveScale * uiScale;
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

			ImVec2 textPos(UIRenderContext::MapPoint(screenRect.x, screenRect.y).x,
			               UIRenderContext::MapPoint(screenRect.x, screenRect.y).y);
			const float scaledWidth = UIRenderContext::MapSizeX(screenRect.z);
			const float scaledHeight = UIRenderContext::MapSizeY(screenRect.w);

			switch (alignment) {
			case TextAlignment::Center:
				textPos.x += (scaledWidth - textSize.x) * 0.5f;
				textPos.y += (scaledHeight - textSize.y) * 0.5f;
				break;
			case TextAlignment::Right:
				textPos.x += scaledWidth - textSize.x;
				textPos.y += (scaledHeight - textSize.y) * 0.5f;
				break;
			case TextAlignment::Left:
			default:
				textPos.y += (scaledHeight - textSize.y) * 0.5f;
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
