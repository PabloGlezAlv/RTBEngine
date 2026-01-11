#include "UIRenderContext.h"
#include <imgui.h>

namespace RTBEngine {
	namespace UI {

		// Static member definitions
		ImDrawList* UIRenderContext::CurrentDrawList = nullptr;
		Math::Vector2 UIRenderContext::Offset = Math::Vector2(0.0f, 0.0f);
		bool UIRenderContext::IsValid = false;

		ImDrawList* UIRenderContext::GetDrawList() {
			if (IsValid && CurrentDrawList) {
				return CurrentDrawList;
			}
			return ImGui::GetBackgroundDrawList();
		}

	}
}
