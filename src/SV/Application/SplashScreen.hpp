#pragma once
#include <string_view>

#include "Dahlia/Utilities/Memory.h"

namespace DH
{
	class Texture2D;
}

namespace SV
{
	// The Space Visualiser icon, name and loading progress over the whole window while the scene
	// loads. It takes keyboard focus and mouse input until dismissed, then fades out.
	class SplashScreen
	{
	public:
		// Needs the GPU context (the icon is uploaded to a texture)
		SplashScreen();
		~SplashScreen();

		// Call once a frame before Draw
		void Update(double deltaTime);
		// Draw over everything in the current ImGui frame. `progress` is 0 to 1; `status` says what
		// is loading.
		void Draw(float progress, std::string_view status);

		// Start fading out
		void Dismiss() { dismissed = true; }
		[[nodiscard]] bool IsDismissed() const { return dismissed; }
		// Faded out: it no longer needs drawing
		[[nodiscard]] bool IsFinished() const { return opacity <= 0.0f; }

		// Seconds since the splash screen was created
		[[nodiscard]] double GetAge() const { return age; }

	private:
		sRef<DH::Texture2D> icon;
		double age = 0.0;
		float opacity = 1.0f;
		bool dismissed = false;
	};
}
