#pragma once

// Trackpad information GLFW does not provide. On macOS an AppKit event monitor reports whether
// scrolling comes from a trackpad (precise, continuous deltas) or a mouse wheel (line steps), and
// collects pinch (magnify) gestures. Elsewhere every scroll is treated as a mouse wheel.
namespace SV::Trackpad
{
#ifdef __APPLE__
	void Install();
	void Uninstall();
	// True if the most recent scroll event came from a trackpad or Magic Mouse
	bool LastScrollWasPrecise();
	// Pinch magnification accumulated since the last call (> 0 pinching out)
	float ConsumeMagnification();
#else
	inline void Install() {}
	inline void Uninstall() {}
	inline bool LastScrollWasPrecise() { return false; }
	inline float ConsumeMagnification() { return 0.0f; }
#endif
}
