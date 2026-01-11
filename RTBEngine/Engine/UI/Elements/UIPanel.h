#pragma once
#include "../UIElement.h"
#include "../../Math/Vectors/Vector4.h"
#include "../../Reflection/PropertyMacros.h"

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

			virtual void Render() override;

			// Reflected properties
			Math::Vector4 backgroundColor = Math::Vector4(0.2f, 0.2f, 0.2f, 0.8f);
			Math::Vector4 borderColor = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			float borderThickness = 1.0f;
			bool hasBorder = false;

			RTB_COMPONENT(UIPanel)
		};

	}
}
