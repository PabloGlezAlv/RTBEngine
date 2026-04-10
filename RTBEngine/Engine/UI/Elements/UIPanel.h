#pragma once
#include "../UIElement.h"
#include "../../Math/Vectors/Vector4.h"
#include "../../Reflection/PropertyMacros.h"

struct ImDrawList;

namespace RTBEngine {
	namespace UI {

		class RTB_API UIPanel : public UIElement {
		public:
			UIPanel();
			virtual ~UIPanel();

			UIPanel(const UIPanel&) = delete;
			UIPanel& operator=(const UIPanel&) = delete;

			void SetBackgroundColor(const Math::Vector4& color);
			Math::Vector4 GetBackgroundColor() const { return backgroundColor; }

			void SetBorderColor(const Math::Vector4& color);
			void SetBorderThickness(float thickness);
			void SetHasBorder(bool hasBorder);

			virtual void Render() override;

			// Reflected properties
			Math::Vector4 backgroundColor = Math::Vector4(0.2f, 0.2f, 0.2f, 0.8f);
			Math::Vector4 borderColor = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			float borderThickness = 1.0f;
			bool hasBorder = false;
			RTB_COMPONENT(UIPanel)

		protected:
			virtual void OnRenderDecorations(ImDrawList* drawList, float minX, float minY, float maxX, float maxY) {}

		private:
			void DrawRotatedRect(ImDrawList* drawList, float cx, float cy,
				float halfW, float halfH, float angleDeg, unsigned int color, bool filled);
		};

	}
}
