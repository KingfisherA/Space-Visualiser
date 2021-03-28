#pragma once
#include"SV/OpenGL/OpenGL Context.hpp"
#include "SV/Utilities/SVMemory.hpp"

//
class Application final abstract
{
public:
	//
	static bool Initilise(const char* assemblyName);
	static int Run();
	//
};
