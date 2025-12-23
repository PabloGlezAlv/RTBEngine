#pragma once
#include "../UIElement.h"
#include "../../Math/Vectors/Vector4.h"
#include "../../Rendering/Texture.h"

namespace RTBEngine {
	namespace UI {

		class UIImage : public UIElement {
		public:
			UIImage();
			virtual ~UIImage();

			void SetTexture(Rendering::Texture* tex);
			Rendering::Texture* GetTexture() const { return texture; }

			void SetTint(const Math::Vector4& tint);
			Math::Vector4 GetTint() const { return tintColor; }

			void SetPreserveAspect(bool preserve);
			bool GetPreserveAspect() const { return preserveAspect; }

			virtual const char* GetTypeName() const override { return "UIImage"; }
			virtual void Render() override;

		private:
			Rendering::Texture* texture;
			Math::Vector4 tintColor;
			bool preserveAspect;
		};

	}
}
