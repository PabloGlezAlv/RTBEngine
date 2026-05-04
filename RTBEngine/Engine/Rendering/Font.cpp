#include "Font.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include "../RTBEngine.h"
#include <cfloat>
#include <cstdint>
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
				RTB_WARN("Font already loaded: " + filePath);
				return false;
			}

			ImGuiIO& io = ImGui::GetIO();

			for (int i = 0; i < numSizes; ++i) {
				ImFont* imFont = io.Fonts->AddFontFromFileTTF(path.c_str(), sizes[i]);
				if (!imFont) {
					RTB_ERROR("Failed to load font: " + path + " at size " + std::to_string(sizes[i]));
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

		Math::Vector2 Font::MeasureText(const std::string& text, float size) const {
			ImFont* imFont = GetImFont(size);
			if (!imFont || text.empty()) {
				return Math::Vector2(0.0f, 0.0f);
			}

			// Ask the underlying font backend for the final pixel size of the string.
			ImVec2 measured = imFont->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
			return Math::Vector2(measured.x, measured.y);
		}

		void Font::BuildTextGeometry(const std::string& text, float size, const Math::Vector2& origin, std::vector<FontTextVertex>& outVertices) const {
			outVertices.clear();

			ImFont* imFont = GetImFont(size);
			if (!imFont || text.empty()) return;

			ImFontBaked* bakedFont = imFont->GetFontBaked(size);
			if (!bakedFont || bakedFont->Size <= 0.0f) return;

			// Build glyph quads in 2D canvas pixel coordinates before CanvasSystem maps them to world units.
			float cursorX = origin.x;
			float cursorY = origin.y;
			const float originX = origin.x;
			const float lineHeight = size;
			const float scale = size / bakedFont->Size;

			const char* current = text.c_str();
			const char* end = current + text.size();
			while (current < end) {
				unsigned int codepoint = static_cast<unsigned char>(*current);
				//Normal text
				if (codepoint < 0x80) {
					current += 1;
				}
				else { //Multibyte check
					int bytes = ImTextCharFromUtf8(&codepoint, current, end);
					current += bytes > 0 ? bytes : 1;
				}
				
				//Check line jump
				if (codepoint < 32) {
					if (codepoint == '\n') {
						cursorX = originX;
						cursorY += lineHeight;
					}
					continue;
				}
				
				// Info
				const ImFontGlyph* glyph = bakedFont->FindGlyph(static_cast<ImWchar>(codepoint));
				if (!glyph) continue;

				const float advance = glyph->AdvanceX * scale;
				if (glyph->Visible) {
					// Each visible glyph becomes two triangles sampling its area inside the font atlas.
					const float x0 = cursorX + glyph->X0 * scale;
					const float x1 = cursorX + glyph->X1 * scale;
					const float y0 = cursorY + glyph->Y0 * scale;
					const float y1 = cursorY + glyph->Y1 * scale;

					outVertices.push_back({ x0, y0, glyph->U0, glyph->V0 });
					outVertices.push_back({ x1, y0, glyph->U1, glyph->V0 });
					outVertices.push_back({ x1, y1, glyph->U1, glyph->V1 });
					outVertices.push_back({ x0, y0, glyph->U0, glyph->V0 });
					outVertices.push_back({ x1, y1, glyph->U1, glyph->V1 });
					outVertices.push_back({ x0, y1, glyph->U0, glyph->V1 });
				}

				cursorX += advance;
			}
		}

		unsigned int Font::GetAtlasTextureID(float size) const {
			ImFont* imFont = GetImFont(size);
			if (!imFont || !imFont->OwnerAtlas) return 0;

			// Expose the uploaded atlas texture without leaking ImGui-specific types to UI systems.
			ImTextureID textureID = imFont->OwnerAtlas->TexRef.GetTexID();
			if (textureID == ImTextureID_Invalid) return 0;

			return static_cast<unsigned int>(static_cast<std::uintptr_t>(textureID));
		}

	}
}
