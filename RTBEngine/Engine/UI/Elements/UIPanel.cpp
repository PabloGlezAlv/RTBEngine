#include "UIPanel.h"
#include "../UIRenderContext.h"
#include <imgui.h>

namespace RTBEngine {
	namespace UI {

		using ThisClass = UIPanel;
		RTB_REGISTER_COMPONENT(UIPanel)
			RTB_PROPERTY_COLOR(backgroundColor)
			RTB_PROPERTY_COLOR(borderColor)
			RTB_PROPERTY(borderThickness)
			RTB_PROPERTY(hasBorder)
			RTB_PROPERTY(isVisible)
		RTB_END_REGISTER(UIPanel)

		UIPanel::UIPanel()
			: backgroundColor(1.0f, 1.0f, 1.0f, 0.5f)
			, borderColor(1.0f, 1.0f, 1.0f, 1.0f) {
		}

		UIPanel::~UIPanel() {
		}

		void UIPanel::SetBackgroundColor(const Math::Vector4& color) {
			backgroundColor = color;
		}

		void UIPanel::SetBorderColor(const Math::Vector4& color) {
			borderColor = color;
		}

		void UIPanel::SetBorderThickness(float thickness) {
			borderThickness = thickness;
		}

		void UIPanel::SetHasBorder(bool border) {
			hasBorder = border;
		}

		void UIPanel::Render() {
			if (!isVisible) return;

			Math::Vector4 screenRect = rectTransform->GetScreenRect();

			ImDrawList* drawList = UIRenderContext::GetDrawList();
			Math::Vector2 offset = UIRenderContext::Offset;

			ImVec2 min(screenRect.x + offset.x, screenRect.y + offset.y);
			ImVec2 max(screenRect.x + screenRect.z + offset.x, screenRect.y + screenRect.w + offset.y);

			ImU32 bgColor = IM_COL32(
				static_cast<int>(backgroundColor.x * 255),
				static_cast<int>(backgroundColor.y * 255),
				static_cast<int>(backgroundColor.z * 255),
				static_cast<int>(backgroundColor.w * 255)
			);

			drawList->AddRectFilled(min, max, bgColor);

			if (hasBorder) {
				ImU32 bColor = IM_COL32(
					static_cast<int>(borderColor.x * 255),
					static_cast<int>(borderColor.y * 255),
					static_cast<int>(borderColor.z * 255),
					static_cast<int>(borderColor.w * 255)
				);

				drawList->AddRect(min, max, bColor, 0.0f, 0, borderThickness);
			}
		}

	}
}
