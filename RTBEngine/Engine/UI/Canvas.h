#pragma once
#include "../RTBEngineAPI.h"
#include "../ECS/Component.h"
#include "../Math/Vectors/Vector2.h"
#include "../Reflection/PropertyMacros.h"
#include "UIElement.h"
#include <vector>
#include <cstdint>

namespace RTBEngine {
	namespace UI {

#pragma warning(push)
#pragma warning(disable: 4251)
		class RTB_API Canvas : public ECS::Component {
		public:
			enum class RenderMode {
				ScreenSpaceOverlay,  // UI always on top, ignores 3D
				ScreenSpaceCamera,   // UI rendered by specific camera
				WorldSpace          // UI positioned in 3D world
			};

			Canvas();
			virtual ~Canvas();

			void PrepareForHitTest(const Math::Vector2& screenSize);
		void RenderCanvas(const Math::Vector2& screenSize);

			RenderMode GetRenderMode() const { return renderMode; }
			void SetRenderMode(RenderMode mode) { renderMode = mode; }

			Math::Vector2 GetCanvasSize() const { return canvasSize; }
			void SetCanvasSize(const Math::Vector2& size) { canvasSize = size; }

			float GetPixelsPerUnit() const { return pixelsPerUnit; }
			void SetPixelsPerUnit(float value);

			int GetSortOrder() const { return sortOrder; }
			void SetSortOrder(int order) { sortOrder = order; }

			const std::vector<UIElement*>& GetUIElements() const { return cachedUIElements; }

			virtual void OnAwake() override;
			virtual void OnStart() override;
			virtual void OnDestroy() override;

			RTB_COMPONENT(Canvas)

		public:
			void MarkHierarchyDirty() { hierarchyDirty = true; }

		private:
			void CollectUIElementsIfNeeded();
			void ApplyLayoutGroups();
			void UpdateRectTransforms(const Math::Vector2& screenSize);

			RenderMode renderMode = RenderMode::ScreenSpaceOverlay;
			Math::Vector2 canvasSize;
			float pixelsPerUnit = 100.0f;
			int sortOrder = 0;
			std::vector<UIElement*> cachedUIElements;
			bool isInitialized = false;
			bool hierarchyDirty = true;
			uint32_t lastHierarchyVersion = 0;

			friend struct Canvas_TypeRegistrar;
		};
#pragma warning(pop)

	}
}
