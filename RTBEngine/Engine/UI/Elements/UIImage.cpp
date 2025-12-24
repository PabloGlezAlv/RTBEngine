#include "UIImage.h"
#include <imgui.h>

namespace RTBEngine {
	namespace UI {

		UIImage::UIImage()
			: texture(nullptr)
			, tintColor(1.0f, 1.0f, 1.0f, 1.0f)
			, preserveAspect(true)
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

			Math::Vector4 screenRect = rectTransform->GetScreenRect();

			ImDrawList* drawList = ImGui::GetBackgroundDrawList();

			ImVec2 min(screenRect.x, screenRect.y);
			ImVec2 max(screenRect.x + screenRect.z, screenRect.y + screenRect.w);

			ImVec2 uv0(0.0f, 1.0f);
			ImVec2 uv1(1.0f, 0.0f);

			ImU32 tint = IM_COL32(
				static_cast<int>(tintColor.x * 255),
				static_cast<int>(tintColor.y * 255),
				static_cast<int>(tintColor.z * 255),
				static_cast<int>(tintColor.w * 255)
			);

			GLuint texID = texture->GetID();
			drawList->AddImage((ImTextureID)(intptr_t)texID, min, max, uv0, uv1, tint);
		}

	}
}
