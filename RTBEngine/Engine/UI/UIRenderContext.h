#pragma once
#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector2.h"

struct ImDrawList;

namespace RTBEngine {
	namespace UI {

		// Rendering context for UI elements
		// Set before rendering canvases to control where UI is drawn
		struct RTB_API UIRenderContext {
			static ImDrawList* CurrentDrawList;	// DrawList to use (nullptr = use GetBackgroundDrawList)
			static Math::Vector2 Offset;		// Position offset for all UI elements
			static float ScaleX;				// Horizontal scale from logical canvas pixels to screen pixels
			static float ScaleY;				// Vertical scale from logical canvas pixels to screen pixels
			static bool IsValid;				// Whether context is active

			static float UniformScale() {
				return (ScaleX + ScaleY) * 0.5f;
			}

			static Math::Vector2 MapPoint(float x, float y) {
				return Math::Vector2(x * ScaleX + Offset.x, y * ScaleY + Offset.y);
			}

			static float MapSizeX(float value) {
				return value * ScaleX;
			}

			static float MapSizeY(float value) {
				return value * ScaleY;
			}

			// Set the render context before rendering UI
			static void Begin(ImDrawList* drawList, const Math::Vector2& offset, float scale = 1.0f) {
				Begin(drawList, offset, Math::Vector2(scale, scale));
			}

			static void Begin(ImDrawList* drawList, const Math::Vector2& offset, const Math::Vector2& scale) {
				CurrentDrawList = drawList;
				Offset = offset;
				ScaleX = scale.x;
				ScaleY = scale.y;
				IsValid = true;
			}

			// Clear the render context after rendering
			static void End() {
				CurrentDrawList = nullptr;
				Offset = Math::Vector2(0.0f, 0.0f);
				ScaleX = 1.0f;
				ScaleY = 1.0f;
				IsValid = false;
			}

			// Get the current DrawList (falls back to BackgroundDrawList if no context)
			static ImDrawList* GetDrawList();
		};

	}
}
