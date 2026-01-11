#pragma once
#include "../Math/Vectors/Vector2.h"

struct ImDrawList;

namespace RTBEngine {
	namespace UI {

		// Rendering context for UI elements
		// Set before rendering canvases to control where UI is drawn
		struct UIRenderContext {
			static ImDrawList* CurrentDrawList;	// DrawList to use (nullptr = use GetBackgroundDrawList)
			static Math::Vector2 Offset;		// Position offset for all UI elements
			static bool IsValid;				// Whether context is active

			// Set the render context before rendering UI
			static void Begin(ImDrawList* drawList, const Math::Vector2& offset) {
				CurrentDrawList = drawList;
				Offset = offset;
				IsValid = true;
			}

			// Clear the render context after rendering
			static void End() {
				CurrentDrawList = nullptr;
				Offset = Math::Vector2(0.0f, 0.0f);
				IsValid = false;
			}

			// Get the current DrawList (falls back to BackgroundDrawList if no context)
			static ImDrawList* GetDrawList();
		};

	}
}
