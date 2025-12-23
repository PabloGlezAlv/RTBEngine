#pragma once
#include "../UIElement.h"
#include "../../Math/Vectors/Vector4.h"

namespace RTBEngine {
	namespace UI {

		class UIPanel : public UIElement {
		public:
			UIPanel();
			virtual ~UIPanel();

			void SetBackgroundColor(const Math::Vector4& color);
			Math::Vector4 GetBackgroundColor() const { return backgroundColor; }

			void SetBorderColor(const Math::Vector4& color);
			void SetBorderThickness(float thickness);
			void SetHasBorder(bool hasBorder);

			virtual const char* GetTypeName() const override { return "UIPanel"; }
			virtual void Render() override;

		private:
			Math::Vector4 backgroundColor;
			Math::Vector4 borderColor;
			float borderThickness;
			bool hasBorder;
		};

	}
}
