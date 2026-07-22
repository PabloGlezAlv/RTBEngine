#include "Font.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include "../RTBEngine.h"
#include "RHI/RenderDevice.h"
#include "RHI/GraphicsAPI.h"
#include <cfloat>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace RTBEngine {
	namespace Rendering {

		namespace {
			struct AtlasGpuCacheEntry {
				RHI::GpuId texture = RHI::kInvalidGpuId;
				int width = 0;
				int height = 0;
				int uniqueId = -1;
				int usedX = -1;
				int usedY = -1;
				int usedW = -1;
				int usedH = -1;
			};

			// ImGui Vulkan TexID is a VkDescriptorSet pointer — not an RHI GpuId.
			// World UI binds through BindTexture2D(GpuId), so we keep an RHI copy of each atlas.
			std::unordered_map<ImFontAtlas*, AtlasGpuCacheEntry>& AtlasGpuCache()
			{
				static std::unordered_map<ImFontAtlas*, AtlasGpuCacheEntry> cache;
				return cache;
			}

			std::vector<unsigned char> ExpandAtlasToRGBA32(const ImTextureData& tex)
			{
				std::vector<unsigned char> rgba(
					static_cast<std::size_t>(tex.Width) * static_cast<std::size_t>(tex.Height) * 4u);
				const unsigned char* src = tex.Pixels;
				if (!src) {
					return rgba;
				}
				if (tex.Format == ImTextureFormat_RGBA32 && tex.BytesPerPixel == 4) {
					std::memcpy(rgba.data(), src, rgba.size());
					return rgba;
				}
				// Alpha8 (and any 1-bpp atlas): white RGB, glyph coverage in A — matches ui_world.frag.
				for (int i = 0, n = tex.Width * tex.Height; i < n; ++i) {
					const unsigned char a = src[i];
					rgba[static_cast<std::size_t>(i) * 4u + 0] = 255;
					rgba[static_cast<std::size_t>(i) * 4u + 1] = 255;
					rgba[static_cast<std::size_t>(i) * 4u + 2] = 255;
					rgba[static_cast<std::size_t>(i) * 4u + 3] = a;
				}
				return rgba;
			}

			unsigned char SampleMaxAlpha(const std::vector<unsigned char>& rgba, int width, int height,
			                             const ImTextureRect& used)
			{
				unsigned char maxA = 0;
				const int x0 = (std::max)(0, static_cast<int>(used.x));
				const int y0 = (std::max)(0, static_cast<int>(used.y));
				const int x1 = (std::min)(width, static_cast<int>(used.x) + static_cast<int>(used.w));
				const int y1 = (std::min)(height, static_cast<int>(used.y) + static_cast<int>(used.h));
				for (int y = y0; y < y1; ++y) {
					for (int x = x0; x < x1; ++x) {
						const unsigned char a = rgba[static_cast<std::size_t>((y * width + x) * 4 + 3)];
						if (a > maxA) maxA = a;
					}
				}
				return maxA;
			}

			RHI::GpuId EnsureAtlasGpuTexture(ImFontAtlas* atlas)
			{
				if (!atlas || !RHI::RenderDevice::HasDevice()) {
					return RHI::kInvalidGpuId;
				}

				// Prefer live TexData (ImGui 1.92 dynamic atlas). Avoid GetTexDataAsRGBA32 —
				// it forces a rebuild/format change and desyncs from the backend atlas UVs.
				ImTextureData* tex = atlas->TexData;
				if (!tex || !tex->Pixels || tex->Width <= 0 || tex->Height <= 0) {
					return RHI::kInvalidGpuId;
				}

				auto& device = RHI::RenderDevice::Get();
				AtlasGpuCacheEntry& entry = AtlasGpuCache()[atlas];
				const bool needsUpload = (entry.texture == RHI::kInvalidGpuId)
					|| entry.uniqueId != tex->UniqueID
					|| entry.width != tex->Width
					|| entry.height != tex->Height
					|| entry.usedX != tex->UsedRect.x
					|| entry.usedY != tex->UsedRect.y
					|| entry.usedW != tex->UsedRect.w
					|| entry.usedH != tex->UsedRect.h;
				if (needsUpload) {
					std::vector<unsigned char> rgba = ExpandAtlasToRGBA32(*tex);
					if (entry.texture == RHI::kInvalidGpuId) {
						entry.texture = device.CreateTexture2D();
					}
					device.SetTexture2DData(entry.texture, RHI::TextureFormat::RGBA8,
						tex->Width, tex->Height, rgba.data(), false);
					device.SetTexture2DFilter(entry.texture,
						RHI::TextureFilter::Linear, RHI::TextureFilter::Linear);
					device.SetTexture2DWrap(entry.texture,
						RHI::TextureWrap::ClampToEdge, RHI::TextureWrap::ClampToEdge);
					entry.width = tex->Width;
					entry.height = tex->Height;
					entry.uniqueId = tex->UniqueID;
					entry.usedX = tex->UsedRect.x;
					entry.usedY = tex->UsedRect.y;
					entry.usedW = tex->UsedRect.w;
					entry.usedH = tex->UsedRect.h;
				}
				return entry.texture;
			}
		}

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

			if (!ImGui::GetCurrentContext()) {
				RTB_WARN("Font::LoadFromFile skipped (no ImGui context): " + path);
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

			if (!RHI::RenderDevice::HasDevice()) {
				return 0;
			}

			auto& device = RHI::RenderDevice::Get();
			// OpenGL ImGui TexID is a GLuint and BindTexture2D binds that name directly.
			if (device.GetAPI() == RHI::GraphicsAPI::OpenGL) {
				ImTextureID textureID = imFont->OwnerAtlas->TexRef.GetTexID();
				if (textureID == ImTextureID_Invalid) return 0;
				return static_cast<unsigned int>(static_cast<std::uintptr_t>(textureID));
			}

			const RHI::GpuId atlasGpuId = EnsureAtlasGpuTexture(imFont->OwnerAtlas);
			return atlasGpuId;
		}

	}
}
