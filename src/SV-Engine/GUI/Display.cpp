#include"SV-Engine/Precompiled Global.hpp"
#include"Display.hpp"
#include"SV-Engine/Utilities/GlobalItems.hpp"
#include"SV-Engine/Utilities/SVUtilities.hpp"

#include<iostream>
#include<functional>

#include<GLFW/glfw3.h>

namespace SV
{
	unsigned int Display::windowCount = 0;
	//
	Display::Display(unsigned int widthIn, unsigned int heightIn, const SV::StringView windowTitle)
	{
		this->width = widthIn;
		this->height = heightIn;

		windowID = glfwCreateWindow(widthIn, heightIn, "Space Visualiser", nullptr, nullptr);
		if (!windowID)
		{
			SV_FATAL_T(SV::Display::Display, "GLFW cannot create a window with OpenGL context selected. Please see error callback for more information.");
			throw std::runtime_error("GLFW cannot create a window with OpenGL context selected.");
		}

		//Width and height track the framebuffer (in pixels) rather than the window (in screen coordinates),
		//as they differ on high-DPI displays and the viewport must match the framebuffer.
		int fbWidth = 0, fbHeight = 0;
		glfwGetFramebufferSize(windowID, &fbWidth, &fbHeight);
		this->width = fbWidth;
		this->height = fbHeight;

		glfwMakeContextCurrent(windowID);
		glfwSwapInterval(DEFAULT_SWAP_INTERVAL);
		glfwSetWindowUserPointer(windowID, (void*)this);

		#ifdef _WIN32
		Display::SetWindowIconFromRes(windowTitle, smallIcon, largeIcon);
		#endif

		windowCount++;
	}
	//
	Display::~Display()
	{
		this->Destroy();
	}
	//
	int Display::Initilise(int majorVersion, int minorVersion)
	{
		glfwSetErrorCallback(&SV::Display::DisplayErrorCallback);
		int errorCode = glfwInit();
		if (errorCode == GLFW_FALSE)
		{
			glfwTerminate();
			return errorCode;
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, majorVersion);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minorVersion);

		return errorCode;
	}
	//
	void Display::DisplayErrorCallback(int errorCode, const char* message)
	{
		Console::SetConsoleColour(ConsoleColour::Red);
		SV::String consoleMessage = "[GLFW Error] A fatal GLFW error occured. Error code = "_str + SV::NumberToString(errorCode)+ ", Message = " + message;
		Console::WriteLine(consoleMessage);
	}
	//
	#ifdef _WIN32
	void Display::SetWindowIconFromRes(const SV::StringView windowTitle, const HANDLE smallIcon, const HANDLE largeIcon)
	{
		HWND windowHandle = FindWindow(NULL, _T(windowTitle.GetStringArray()));
		SendMessage(windowHandle, (unsigned int)WM_SETICON, ICON_SMALL, (LPARAM)smallIcon);
		SendMessage(windowHandle, (unsigned int)WM_SETICON, ICON_BIG, (LPARAM)largeIcon);
	}
	#endif
	//
	void Display::AddMouseClickEvent(MouseClickHandler h)
	{
		this->clickHandle = h;
		glfwSetMouseButtonCallback(windowID, OnMouseClick);
	}
	void Display::AddMouseMotionEvent(CursorMotionHandler h)
	{
		this->motionHandle = h;
		glfwSetCursorPosCallback(windowID, OnCursorMotion);
	}
	void Display::AddWindowResizeEvent(ResizedHandler h)
	{
		this->resizeHandle = h;
		glfwSetFramebufferSizeCallback(windowID, OnWindowResized);

		//Call once handle is bound to ensure the screen viewport is updated
		OnWindowResized(windowID, width, height);
	}
	void Display::AddScrollEvent(ScrollHandler h)
	{
		this->scrollHandle = h;
		glfwSetScrollCallback(windowID, OnScroll);
	}
	void Display::AddKeyboardEvent(KeyboardHandler h)
	{
		this->keyHandle = h;
		glfwSetKeyCallback(windowID, &OnKeyPress);
	}
	//
	void Display::OnWindowResized(GLFWwindow* window, int newWidth, int newHeight)
	{
		Display* display = (Display*)glfwGetWindowUserPointer(window);

		//Minimised windows report a 0x0 framebuffer; skip to avoid a degenerate viewport and projection
		if (newWidth <= 0 || newHeight <= 0) return;

		display->width = newWidth;
		display->height = newHeight;

		display->resizeHandle(display, newWidth, newHeight);

		//No buffer swap here: this is called outside the render loop, so swapping would
		//present a back buffer that hasn't been drawn to and cause a flicker.
	}
	void Display::OnCursorMotion(GLFWwindow* window, double x, double y)
	{
		Display* display = (Display*)glfwGetWindowUserPointer(window);

		display->mouseX = x;
		display->mouseY = y;

		display->mouseDX = display->mouseX - display->lastMouseX;
		display->mouseDY = display->mouseY - display->lastMouseY;

		display->lastMouseX = x;
		display->lastMouseY = y;

		display->motionHandle(display, x, display->mouseDX, y, display->mouseDY);
	}
	void Display::OnMouseClick(GLFWwindow* window, int button, int action, int mods)
	{
		Display* display = (Display*)glfwGetWindowUserPointer(window);

		display->currentMouseButton = button;
		display->currentMouseAction = action;
		display->currentMouseMods = mods;

		display->clickHandle(display, button, action, mods);
	}
	void Display::OnScroll(GLFWwindow* window, double xoffset, double yoffset)
	{
		Display* display = (Display*)glfwGetWindowUserPointer(window);

		double currentScrollTime = glfwGetTime();
		display->deltaScroll = currentScrollTime - display->lastScrollTime;
		display->lastScrollTime = currentScrollTime;

		display->scrollHandle(display, xoffset, yoffset);
	}
	void Display::OnKeyPress(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		Display* display = (Display*)glfwGetWindowUserPointer(window);

		display->keyHandle(display, key, scancode, action, mods);
	}
	//
	void Display::RefreshViewport()
	{
		glViewport(0, 0, width, height);
		FlushScreen();
	}
	void Display::PollEvents()
	{
		currentFrameTime = glfwGetTime();
		deltaFrameTime = currentFrameTime - lastFrameTime;
		lastFrameTime = currentFrameTime;
		glfwPollEvents();
	}
	void Display::FlushScreen()
	{
		glfwSwapBuffers(windowID);
	}
	//
	void Display::Destroy()
	{
		if (windowID != nullptr)
		{
			glfwDestroyWindow(windowID);
			windowID = nullptr;
			windowCount--;
		}

		if (windowCount == 0)
		{
			glfwTerminate();
		}
	}
	//
}