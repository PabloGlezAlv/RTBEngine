#include "UIImage.h"
#include "../UIRenderContext.h"
#include <imgui.h>

namespace RTBEngine {
	namespace UI {

		using ThisClass = UIImage;
		RTB_REGISTER_COMPONENT(UIImage)
			RTB_PROPERTY_TEXTURE(texture)
			RTB_PROPERTY_COLOR(tintColor)
			RTB_PROPERTY(preserveAspect)
			{ using ThisClass = UIElement; RTB_PROPERTY(isVisible) }
			{ using ThisClass = UIElement; RTB_PROPERTY(raycastTarget) }
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchorMin)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchorMax)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, pivot)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchoredPosition)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, sizeDelta)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, rotation)
			RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, scale)
		RTB_END_REGISTER(UIImage)

		UIImage::UIImage()
		{
		}

		UIImage::~UIImage() {
		}

		void UIImage::SetTexture(Rendering::Texture* tex) {
			texture = tex;
		}

		void UIImage::SetTint(const Math::Vector4& tint) {
			tintColor = tint;
		}

		void UIImage::SetPreserveAspect(bool preserve) {
			preserveAspect = preserve;
		}

		void UIImage::Render() {
			if (!isVisible) return;
			if (!texture) return;

			Math::Vector4 screenRect = rectTransform.GetWorldRect();

			ImDrawList* drawList = UIRenderContext::GetDrawList();
			Math::Vector2 minPt = UIRenderContext::MapPoint(screenRect.x, screenRect.y);
			Math::Vector2 maxPt = UIRenderContext::MapPoint(screenRect.x + screenRect.z, screenRect.y + screenRect.w);

			ImVec2 min(minPt.x, minPt.y);
			ImVec2 max(maxPt.x, maxPt.y);

			ImVec2 uv0(0.0f, 1.0f);
			ImVec2 uv1(1.0f, 0.0f);

			ImU32 tint = IM_COL32(
				static_cast<int>(tintColor.x * 255),
				static_cast<int>(tintColor.y * 255),
				static_cast<int>(tintColor.z * 255),
				static_cast<int>(tintColor.w * 255)
			);

			unsigned int texID = texture->GetID();
			drawList->AddImage((ImTextureID)(intptr_t)texID, min, max, uv0, uv1, tint);
		}

	}
}
