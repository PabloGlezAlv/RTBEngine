#pragma once
#include "../../Math/Vectors/Vector2.h"

namespace RTBEngine {
	namespace Scene {
		class GameObject;
	}

	namespace UI {

		struct PointerEventData {
			Math::Vector2 position;
			Math::Vector2 delta;
			Scene::GameObject* pointerEnter = nullptr;
			Scene::GameObject* pointerPress = nullptr;
			int button = 0;
		};

	}
}
