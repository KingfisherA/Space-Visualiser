#include"SV-Engine/Precompiled Global.hpp"
#include<cstdio>
#include<iostream>
#include"OpenGL Context.hpp"
#include"SV-Engine/OpenGL/VAO.hpp"
#include"ShaderProgram.hpp"
#include"FBO.hpp"
#include"Texture.hpp"
#include"SV-Engine/Utilities/SVUtilities.hpp"


namespace SV
{
	//
	//
	const VAO* OpenGLContext::currentVAO = nullptr;
	const iVBO* OpenGLContext::currentVBO = nullptr;
	const FBO* OpenGLContext::currentFBO = nullptr;
	const ShaderProgram* OpenGLContext::currentProgram = nullptr;
	//
	void __stdcall OpenGLContext::GLErrorCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
	{
		bool isError = type == GL_DEBUG_TYPE_ERROR;

		SV::Console::SetConsoleColour(isError ? SV::ConsoleColour::BrightRed : SV::ConsoleColour::BrightBlue);
		std::cout << (isError ? "[Open GL Error]" : "[Open GL Information]") << " Message Type = 0x" << type << " Severity = " << severity << " Message = " << message << "\n";
	}
	//
	bool OpenGLContext::InitiliseOpenGL()
	{
		GLenum error = glewInit();

		if (error != GLEW_OK)
		{
			Console::SetConsoleColour(ConsoleColour::Red);
			std::cout << "GLEW could not initilise. Error information: " << glewGetErrorString(error) << std::endl;
			return false;
		}

		Console::SetConsoleColour(ConsoleColour::Green);
		std::cout << "GLEW sucessfully initilised. Using GLEW version " << glewGetString(GLEW_VERSION) << std::endl;


		if (GLEW_VERSION_4_6) Console::SetConsoleColour(ConsoleColour::Green);	 std::cout << "OpenGL version 4.6 is supported!" << std::endl;

		glEnable(GL_TEXTURE_2D);

		glEnable(GL_BLEND);
		OpenGLContext::EnableAlphaBlending();

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);

		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		// glEnable(GL_DEBUG_OUTPUT);
		// glDebugMessageCallback(GLErrorCallback, 0);

		return true;
	}
	//
}