#pragma once
#include "../../Math/Vectors/Vector2.h"

namespace RTBEngine {
	namespace ECS {
		class GameObject;
	}

	namespace UI {

		struct PointerEventData {
			Math::Vector2 position;
			Math::Vector2 delta;
			ECS::GameObject* pointerEnter = nullptr;
			ECS::GameObject* pointerPress = nullptr;
			int button = 0;
		};

	}
}
