#pragma once
#include "../RTBEngineAPI.h"
#include <string>
#include <map>

struct ImFont;

namespace RTBEngine {
	namespace Rendering {

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
