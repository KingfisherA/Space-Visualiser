#include "SplashScreen.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <string>

#include <imgui.h>

#include "Dahlia/Graphics/Texture.h"

// A private copy of stb_image's PNG decoder, to decode the embedded icon
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include "Dahlia/Graphics/stb/stb_image.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace SV
{
	namespace
	{
		// Icons/SpaceVisualiser Icon.png, embedded by sv_embed_files (see src/SV/CMakeLists.txt)
		const unsigned char kIconPng[] =
#include "SpaceVisualiser Icon.png.inc"
			;

		// Sizes in points
		constexpr float ICON_SIZE = 256.0f;
		constexpr float TITLE_SIZE = 34.0f;
		constexpr float TEXT_SIZE = 15.0f;
		constexpr float PROGRESS_WIDTH = 260.0f;
		constexpr float PROGRESS_HEIGHT = 4.0f;
		constexpr float FADE_SECONDS = 0.5f;

		// The icon's background: a deep blue, darker towards the bottom
		constexpr ImU32 BACKGROUND_TOP = IM_COL32(18, 32, 74, 255);
		constexpr ImU32 BACKGROUND_BOTTOM = IM_COL32(4, 8, 26, 255);
		constexpr ImU32 TEXT_COLOUR = IM_COL32(236, 240, 255, 255);
		constexpr ImU32 DIM_TEXT_COLOUR = IM_COL32(150, 165, 205, 255);
		constexpr ImU32 PROGRESS_BACK_COLOUR = IM_COL32(255, 255, 255, 40);
		constexpr ImU32 PROGRESS_COLOUR = IM_COL32(255, 196, 92, 255); // The icon's Sun

		ImU32 WithOpacity(const ImU32 colour, const float opacity)
		{
			const auto alpha = (ImU32)((float)((colour >> IM_COL32_A_SHIFT) & 0xFF) * opacity);
			return (colour & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
		}

		// Text centred horizontally on x
		void AddCentredText(ImDrawList& drawList, const float size, const float x, const float y, const ImU32 colour, const std::string_view text)
		{
			ImFont* font = ImGui::GetFont();
			const char* begin = text.data();
			const char* end = text.data() + text.size();
			const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, begin, end);
			drawList.AddText(font, size, ImVec2(std::floor(x - extent.x * 0.5f), y), colour, begin, end);
		}
	}

	SplashScreen::SplashScreen()
	{
		int width = 0, height = 0, channels = 0;
		stbi_uc* pixels = stbi_load_from_memory(kIconPng, (int)sizeof(kIconPng), &width, &height, &channels, 4);
		if (pixels == nullptr)
		{
			std::cerr << "[SV::SplashScreen] Failed to decode the icon: " << stbi_failure_reason() << '\n';
			return;
		}
		icon = DH::Texture2D::Create({ (size_t)width, (size_t)height }, pixels, DH::Format::RGBA8);
		stbi_image_free(pixels);
	}

	SplashScreen::~SplashScreen() = default;

	void SplashScreen::Update(const double deltaTime)
	{
		age += deltaTime;
		if (dismissed)
			opacity = std::max(opacity - (float)deltaTime / FADE_SECONDS, 0.0f);
	}

	void SplashScreen::Draw(const float progress, const std::string_view status)
	{
		if (IsFinished())
			return;

		// A window over the whole display, on top and holding focus until dismissed so input doesn't
		// reach the scene while it loads
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
								 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav;
		if (dismissed)
			flags |= ImGuiWindowFlags_NoInputs;
		else
			ImGui::SetNextWindowFocus();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::Begin("##SplashScreen", nullptr, flags);
		ImGui::PopStyleVar();

		ImDrawList& drawList = *ImGui::GetWindowDrawList();
		// Ease out the fade
		const float alpha = opacity * opacity;
		const ImVec2 min = viewport->Pos;
		const ImVec2 max(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
		drawList.AddRectFilledMultiColor(min, max, WithOpacity(BACKGROUND_TOP, alpha), WithOpacity(BACKGROUND_TOP, alpha),
										 WithOpacity(BACKGROUND_BOTTOM, alpha), WithOpacity(BACKGROUND_BOTTOM, alpha));

		// The icon, then the title, version, progress bar and status below it, centred together
		const float iconSize = std::min(ICON_SIZE, 0.45f * std::min(viewport->Size.x, viewport->Size.y));
		const float blockHeight = iconSize + TITLE_SIZE + TEXT_SIZE + PROGRESS_HEIGHT + TEXT_SIZE + 52.0f;
		const float centreX = viewport->Pos.x + viewport->Size.x * 0.5f;
		float y = std::floor(viewport->Pos.y + (viewport->Size.y - blockHeight) * 0.5f);

		if (icon != nullptr)
		{
			const ImVec2 iconMin(std::floor(centreX - iconSize * 0.5f), y);
			drawList.AddImage((ImTextureID)icon->GetImGuiID(), iconMin, ImVec2(iconMin.x + iconSize, iconMin.y + iconSize),
							  ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), WithOpacity(IM_COL32_WHITE, alpha));
		}
		y += iconSize + 12.0f;

		AddCentredText(drawList, TITLE_SIZE, centreX, y, WithOpacity(TEXT_COLOUR, alpha), "Space Visualiser");
		y += TITLE_SIZE + 4.0f;
		AddCentredText(drawList, TEXT_SIZE, centreX, y, WithOpacity(DIM_TEXT_COLOUR, alpha), "Version " SV_VERSION);
		y += TEXT_SIZE + 24.0f;

		const ImVec2 barMin(std::floor(centreX - PROGRESS_WIDTH * 0.5f), y);
		const ImVec2 barMax(barMin.x + PROGRESS_WIDTH, barMin.y + PROGRESS_HEIGHT);
		const float rounding = PROGRESS_HEIGHT * 0.5f;
		drawList.AddRectFilled(barMin, barMax, WithOpacity(PROGRESS_BACK_COLOUR, alpha), rounding);
		const float filled = PROGRESS_WIDTH * std::clamp(progress, 0.0f, 1.0f);
		if (filled > 0.0f)
			drawList.AddRectFilled(barMin, ImVec2(barMin.x + filled, barMax.y), WithOpacity(PROGRESS_COLOUR, alpha), rounding);
		y += PROGRESS_HEIGHT + 12.0f;

		AddCentredText(drawList, TEXT_SIZE, centreX, y, WithOpacity(DIM_TEXT_COLOUR, alpha), status);

		ImGui::End();
	}
}
