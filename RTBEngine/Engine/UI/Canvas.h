#pragma once
#include "../ECS/Component.h"
#include "../Math/Vectors/Vector2.h"
#include "UIElement.h"
#include <vector>

namespace RTBEngine {
	namespace UI {

		class Canvas : public ECS::Component {
		public:
			enum class RenderMode {
				ScreenSpaceOverlay,  // UI always on top, ignores 3D
				ScreenSpaceCamera,   // UI rendered by specific camera
				WorldSpace          // UI positioned in 3D world
			};

			Canvas();
			virtual ~Canvas();

			void RenderCanvas(const Math::Vector2& screenSize);

			RenderMode GetRenderMode() const { return renderMode; }
			void SetRenderMode(RenderMode mode) { renderMode = mode; }

			Math::Vector2 GetCanvasSize() const { return canvasSize; }
			void SetCanvasSize(const Math::Vector2& size) { canvasSize = size; }

			int GetSortOrder() const { return sortOrder; }
			void SetSortOrder(int order) { sortOrder = order; }

			const std::vector<UIElement*>& GetUIElements() const { return cachedUIElements; }

			virtual const char* GetTypeName() const override { return "Canvas"; }

			virtual void OnAwake() override;
			virtual void OnStart() override;
			virtual void OnUpdate(float deltaTime) override;
			virtual void OnDestroy() override;

		private:
			void CollectUIElements();
			void UpdateRectTransforms(const Math::Vector2& screenSize);

			RenderMode renderMode = RenderMode::ScreenSpaceOverlay;
			Math::Vector2 canvasSize;
			int sortOrder = 0;
			std::vector<UIElement*> cachedUIElements;
			bool isInitialized = false;
		};

	}
}
