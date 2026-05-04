#pragma once
#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector2.h"
#include <string>
#include <map>
#include <vector>

struct ImFont;

namespace RTBEngine {
	namespace Rendering {

		struct FontTextVertex {
			float x = 0.0f;
			float y = 0.0f;
			float u = 0.0f;
			float v = 0.0f;
		};

#pragma warning(push)
#pragma warning(disable: 4251)
		class RTB_API Font {
		public:
			Font();
			~Font();

			Font(const Font&) = delete;
			Font& operator=(const Font&) = delete;

			bool LoadFromFile(const std::string& path, const float* sizes, int numSizes);

			ImFont* GetImFont(float size) const;
			Math::Vector2 MeasureText(const std::string& text, float size) const;
			void BuildTextGeometry(const std::string& text, float size, const Math::Vector2& origin, std::vector<FontTextVertex>& outVertices) const;
			unsigned int GetAtlasTextureID(float size) const;
			const std::string& GetPath() const { return filePath; }
			bool IsLoaded() const { return isLoaded; }

		private:
			std::string filePath;
			std::map<float, ImFont*> fontSizes;
			bool isLoaded;
		};
#pragma warning(pop)

	}
}
