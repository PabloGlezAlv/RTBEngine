#include "UIPanel.h"
#include "../UIRenderContext.h"
#include <imgui.h>
#include <cmath>

namespace RTBEngine {
	namespace UI {

		using ThisClass = UIPanel;
		RTB_REGISTER_COMPONENT(UIPanel)
			RTB_PROPERTY_COLOR(backgroundColor)
			RTB_PROPERTY_COLOR(borderColor)
			RTB_PROPERTY(borderThickness)
			RTB_PROPERTY(hasBorder)
			{ using ThisClass = UIElement; RTB_PROPERTY(isVisible) }
			{ using ThisClass = UIElement; RTB_PROPERTY(raycastTarget) }
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchorMin)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchorMax)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, pivot)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchoredPosition)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, sizeDelta)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, rotation)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, scale)
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

		void UIPanel::DrawRotatedRect(ImDrawList* drawList, float cx, float cy,
			float halfW, float halfH, float angleDeg, unsigned int color, bool filled)
		{
			float angle = angleDeg * 3.14159265f / 180.0f;
			float cosA = std::cos(angle);
			float sinA = std::sin(angle);

			// Corners relative to center before rotation: TL, TR, BR, BL
			float cornersX[4] = { -halfW,  halfW,  halfW, -halfW };
			float cornersY[4] = { -halfH, -halfH,  halfH,  halfH };

			ImVec2 pts[4];
			for (int i = 0; i < 4; ++i) {
				pts[i].x = cornersX[i] * cosA - cornersY[i] * sinA + cx;
				pts[i].y = cornersX[i] * sinA + cornersY[i] * cosA + cy;
			}

			if (filled) {
				drawList->AddQuadFilled(pts[0], pts[1], pts[2], pts[3], color);
			} else {
				drawList->AddQuad(pts[0], pts[1], pts[2], pts[3], color, borderThickness);
			}
		}

		void UIPanel::Render() {
			if (!isVisible) return;
			if (backgroundColor.w <= 0.001f && !hasBorder) return;

			Math::Vector4 screenRect = rectTransform.GetWorldRect();

			ImDrawList* drawList = UIRenderContext::GetDrawList();
			float rx = UIRenderContext::MapPoint(screenRect.x, screenRect.y).x;
			float ry = UIRenderContext::MapPoint(screenRect.x, screenRect.y).y;
			float rw = UIRenderContext::MapSizeX(screenRect.z);
			float rh = UIRenderContext::MapSizeY(screenRect.w);

			ImVec2 min(rx, ry);
			ImVec2 max(rx + rw, ry + rh);

			float cx = rx + rw * 0.5f;
			float cy = ry + rh * 0.5f;
			float halfW = rw * 0.5f;
			float halfH = rh * 0.5f;

			ImU32 bgColor = IM_COL32(
				static_cast<int>(backgroundColor.x * 255),
				static_cast<int>(backgroundColor.y * 255),
				static_cast<int>(backgroundColor.z * 255),
				static_cast<int>(backgroundColor.w * 255)
			);

			float rot = rectTransform.GetRotation();
			if (rot > 0.01f || rot < -0.01f) {
				DrawRotatedRect(drawList, cx, cy, halfW, halfH, rot, bgColor, true);
			} else {
				drawList->AddRectFilled(min, max, bgColor);
			}

			OnRenderDecorations(drawList, rx, ry, rx + rw, ry + rh);

			if (hasBorder) {
				ImU32 bColor = IM_COL32(
					static_cast<int>(borderColor.x * 255),
					static_cast<int>(borderColor.y * 255),
					static_cast<int>(borderColor.z * 255),
					static_cast<int>(borderColor.w * 255)
				);

				if (rot > 0.01f || rot < -0.01f) {
					DrawRotatedRect(drawList, cx, cy, halfW, halfH, rot, bColor, false);
				} else {
					drawList->AddRect(min, max, bColor, 0.0f, 0, borderThickness);
				}
			}
		}

	}
}
