#pragma once
#include "../../Core/Event.h"
#include "../../ECS/Component.h"
#include "../../Math/Vectors/Vector2.h"
#include "../../Reflection/PropertyMacros.h"
#include "../../RTBEngineAPI.h"
#include "../EventSystem/IPointerDownHandler.h"
#include "../EventSystem/IBeginDragHandler.h"
#include "../EventSystem/IDragHandler.h"
#include "../EventSystem/IEndDragHandler.h"

namespace RTBEngine {
	namespace UI {
		class UIImage;

#pragma warning(push)
#pragma warning(disable: 4251)
		class RTB_API UIJoystick : public ECS::Component,
		                           public IPointerDownHandler,
		                           public IBeginDragHandler,
		                           public IDragHandler,
		                           public IEndDragHandler
		{
		public:
			using ReleasedCallback = Core::Event<Math::Vector2>::Callback;

			UIJoystick();
			virtual ~UIJoystick();

			UIJoystick(const UIJoystick&) = delete;
			UIJoystick& operator=(const UIJoystick&) = delete;

			const Math::Vector2& GetValue() const { return value; }
			bool IsDragging() const { return isDragging; }
			Core::EventSubscription SubscribeToReleased(ReleasedCallback callback);

			virtual void OnStart() override;
			virtual void OnValidate() override;

			void OnPointerDown(const PointerEventData& eventData) override;
			void OnBeginDrag(const PointerEventData& eventData) override;
			void OnDrag(const PointerEventData& eventData) override;
			void OnEndDrag(const PointerEventData& eventData) override;

			UIImage* handleImage = nullptr;
			float deadZone = 0.20f;
			float maxDistance = 0.0f;
			bool interactable = true;

			RTB_COMPONENT(UIJoystick)

		private:
			UIImage* backgroundImage = nullptr;
			Math::Vector2 value = Math::Vector2::Zero();
			Math::Vector2 handleCenterPosition = Math::Vector2::Zero();
			Core::Event<Math::Vector2> releasedEvent;
			bool isDragging = false;
			bool hasHandleCenterPosition = false;

			void ResolveImages();
			void ResetHandle();
			void UpdateValueFromScreenPosition(const Math::Vector2& screenPosition);
			float GetEffectiveRadius() const;
		};
#pragma warning(pop)

	}
}
