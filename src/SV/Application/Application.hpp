#pragma once
#include"SV-Engine/OpenGL/OpenGL Context.hpp"
#include "SV-Engine/Utilities/SVMemory.hpp"

//
class Application final
{
public:
	//
	static bool Initilise(const char* assemblyName);
	static int Run();
	//
};
