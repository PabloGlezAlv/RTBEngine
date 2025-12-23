#include "Font.h"
#include <imgui.h>
#include <iostream>
#include <cmath>

namespace RTBEngine {
	namespace Rendering {

		Font::Font()
			: isLoaded(false)
		{
		}

		Font::~Font() {
		}

		bool Font::LoadFromFile(const std::string& path, const float* sizes, int numSizes) {
			if (isLoaded) {
				std::cerr << "Font already loaded: " << filePath << std::endl;
				return false;
			}

			ImGuiIO& io = ImGui::GetIO();

			for (int i = 0; i < numSizes; ++i) {
				ImFont* imFont = io.Fonts->AddFontFromFileTTF(path.c_str(), sizes[i]);
				if (!imFont) {
					std::cerr << "Failed to load font: " << path << " at size " << sizes[i] << std::endl;
					fontSizes.clear();
					return false;
				}
				fontSizes[sizes[i]] = imFont;
			}

			io.Fonts->Build();

			filePath = path;
			isLoaded = true;

			return true;
		}

		ImFont* Font::GetImFont(float size) const {
			if (!isLoaded) return nullptr;

			auto it = fontSizes.find(size);
			if (it != fontSizes.end()) {
				return it->second;
			}

			float closest = fontSizes.begin()->first;
			float minDiff = std::abs(size - closest);

			for (const auto& pair : fontSizes) {
				float diff = std::abs(size - pair.first);
				if (diff < minDiff) {
					minDiff = diff;
					closest = pair.first;
				}
			}

			return fontSizes.at(closest);
		}

	}
}
