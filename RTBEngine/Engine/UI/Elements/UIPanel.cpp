#include "UIPanel.h"
#include <imgui.h>

namespace RTBEngine {
	namespace UI {

		UIPanel::UIPanel()
			: backgroundColor(1.0f, 1.0f, 1.0f, 0.5f)
			, borderColor(1.0f, 1.0f, 1.0f, 1.0f)
			, borderThickness(1.0f)
			, hasBorder(false)
		{
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

			ImDrawList* drawList = ImGui::GetBackgroundDrawList();

			ImVec2 min(screenRect.x, screenRect.y);
			ImVec2 max(screenRect.x + screenRect.z, screenRect.y + screenRect.w);

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
