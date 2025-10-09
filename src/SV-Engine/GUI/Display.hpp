#pragma once
#include"SV-Engine/OpenGL/OpenGL Context.hpp"
#include"SV-Engine/Utilities/String.hpp"
#include<chrono>
#include<unordered_map>
#include<GLFW/glfw3.h>
// #define GLFW_EXPOSE_NATIVE_WIN32
// #include<GLFW/glfw3native.h>

namespace SV
{
	class Display;
	//
	constexpr unsigned int DEFAULT_SWAP_INTERVAL = 1;

	//
	typedef void (*ResizedHandler) (Display* const sender, int newWidth, int newHeight);
	typedef void (*MouseClickHandler) (Display* const sender, int button, int action, int mods);
	typedef void (*CursorMotionHandler) (Display* const sender, double xPos, double deltaX, double yPos, double deltaY);
	typedef void (*ScrollHandler)(Display* const sender, double xOffset, double detaXOffset);
	typedef void (*KeyboardHandler)(Display* const sender, int key, int scancode, int action, int mods);


	//
	class Display
	{
	private:
		static unsigned int windowCount;
	public:
		//
		//, const HANDLE smallIcon = nullptr, const HANDLE largeIcon = nullptr
		Display(unsigned int widthIn, unsigned int heightIn, const SV::StringView windowTitle = "");
		~Display();
		//
		static int Initilise(int majorVersion, int minorVersion);
		//
		void SetSwapInterval(const unsigned int interval) { glfwSwapInterval(interval); }
		void MakeCurrentContext() { glfwMakeContextCurrent(windowID); }
		static GLFWwindow* GetCurrentContext() { return glfwGetCurrentContext(); }
		static Display* GetCurrentDisplay() { return (Display*)glfwGetWindowUserPointer(GetCurrentContext()); }
		//
		double GetMouseX() const { return mouseX; }
		double GetMouseDX() const { return mouseDX; }
		double GetMouseY() const { return mouseY; }
		double GetMouseDY() const { return mouseDY; }
		//
		double GetDScroll() const { return deltaScroll; }
		//
		//Get the current delta frame time in seconds
		double GetDeltaFrameTime() const noexcept { return deltaFrameTime; }
		template<typename Duration>
		Duration ConvertDeltaFrameTime() const noexcept
		{
			return std::chrono::duration_cast<Duration>(SV::Seconds<double>(deltaFrameTime));
		}
		//
		int GetMouseButton() const { return currentMouseButton; }
		int GetMouseAction() const { return currentMouseAction; }
		//
		unsigned int GetWidth() const { return width; }
		unsigned int GetHeight() const { return height; }
		//
		inline GLFWwindow* GetWindowID() { return windowID; }
		//
		bool IsCloseRequested() const { return (bool)glfwWindowShouldClose(windowID); }
		void RefreshViewport();
		void PollEvents();
		void FlushScreen();
		//
		void AddMouseClickEvent(MouseClickHandler h);
		void AddMouseMotionEvent(CursorMotionHandler h);
		void AddWindowResizeEvent(ResizedHandler h);
		void AddScrollEvent(ScrollHandler h);
		void AddKeyboardEvent(KeyboardHandler h);
		//
		#ifdef _WIN32
		HWND GetWindowsHandle() { return glfwGetWin32Window(windowID); }
		#endif
		void Focus() { glfwFocusWindow(windowID); }
		//
		static unsigned int GetWindowCount() { return windowCount; }
		//
		void Destroy();
		//
	private:
		//
		static void DisplayErrorCallback(int errorCode, const char* message);
		//
		#ifdef _WIN32
		static void SetWindowIconFromRes(const SV::StringView windowTitle, const HANDLE smallIcon, const HANDLE largeIcon);
		#endif
		//
		static void OnWindowResized(GLFWwindow* window, int newWidth, int newHeight);
		static void OnCursorMotion(GLFWwindow* window, double x, double y);
		static void OnMouseClick(GLFWwindow* window, int button, int action, int mods);
		static void OnScroll(GLFWwindow* window, double xoffset, double yoffset);
		static void OnKeyPress(GLFWwindow* window, int key, int scancode, int action, int mods);
		//
		unsigned int width = 0, height = 0;
		//
		GLFWwindow* windowID = nullptr;
		//
		int currentMouseButton = -1;
		int currentMouseAction = -1;
		int currentMouseMods = -1;
		//
		double mouseX = 0.0;
		double lastMouseX = 0.0;
		double mouseDX = 0.0;
		//
		double mouseY = 0;
		double lastMouseY = 0;
		double mouseDY = 0;
		//
		double lastScrollTime = 0.0;
		double deltaScroll = 0.0;
		//
		double lastFrameTime = 0.0;
		double currentFrameTime = 0.0;
		double deltaFrameTime = 0.0;
		//
		ResizedHandler resizeHandle = nullptr;
		CursorMotionHandler motionHandle = nullptr;
		MouseClickHandler clickHandle = nullptr;
		ScrollHandler scrollHandle = nullptr;
		KeyboardHandler keyHandle = nullptr;
		//
	};

}