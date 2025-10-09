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
#ifdef SV_DEBUG
		if (!windowID)
		{
			SV_FATAL_T(SV::Display::Display, "GLFW cannot create a window with OpenGL context selected. Please see error callback for more information.");
		}
#endif // SV_DEBUG
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

		display->width = newWidth;
		display->height = newHeight;

		display->resizeHandle(display, newWidth, newHeight);

		display->FlushScreen();
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