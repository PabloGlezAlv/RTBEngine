#pragma once
#include "../UIElement.h"
#include "../../Math/Vectors/Vector4.h"
#include "../../Rendering/Texture.h"
#include "../../Reflection/PropertyMacros.h"

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

			virtual void Render() override;

			// Reflected properties
			Rendering::Texture* texture = nullptr;
			Math::Vector4 tintColor = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			bool preserveAspect = true;

			RTB_COMPONENT(UIImage)
		};

	}
}
