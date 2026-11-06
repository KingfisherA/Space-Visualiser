#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>

#include "Dahlia/Platform/GPUContext.h"
#include "SV/Application/Application.hpp"
#include "SV/Graphics/AsyncTexture.hpp"
#include "SV/Universe/Catalogue.hpp"

namespace
{
	void PrintUsage(const char* program)
	{
		std::cout << "Usage: " << program << " [--opengl | --metal | --vulkan] [--data <texture directory>] [--catalogues <catalogue directory>]\n";
	}
}

// Entry point to Space Visualiser
int main(int argc, char** argv)
{
	std::cout << "Space Visualiser 1.0\n";

	// Metal by default on macOS, OpenGL 4.1 elsewhere; every Dahlia backend renders the same scene
#ifdef __APPLE__
	DH::GraphicsAPI api = DH::GraphicsAPI::Metal;
	uint32_t major = 0, minor = 0;
#else
	DH::GraphicsAPI api = DH::GraphicsAPI::OpenGL;
	uint32_t major = 4, minor = 1;
#endif
	for (int i = 1; i < argc; i++)
	{
		const std::string_view arg = argv[i];
		if (arg == "--opengl")
		{
			api = DH::GraphicsAPI::OpenGL;
			major = 4;
			minor = 1;
		}
		else if (arg == "--vulkan")
		{
			api = DH::GraphicsAPI::Vulkan;
			major = 1;
			minor = 3;
		}
		else if (arg == "--metal")
		{
			api = DH::GraphicsAPI::Metal;
		}
		else if (arg == "--data" && i + 1 < argc)
		{
			SV::SetTextureDirectory(argv[++i]);
		}
		else if (arg == "--catalogues" && i + 1 < argc)
		{
			SV::SetCatalogueDirectory(argv[++i]);
		}
		else
		{
			PrintUsage(argv[0]);
			return arg == "--help" || arg == "-h" ? EXIT_SUCCESS : EXIT_FAILURE;
		}
	}

	try
	{
		SV::Application app;
		if (!app.Init(api, major, minor))
		{
			std::cerr << "Failed to initialise Space Visualiser\n";
			return EXIT_FAILURE;
		}
		app.Run();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Space Visualiser stopped with an error: " << e.what() << '\n';
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
