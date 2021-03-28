//Vital utilities
#include<Windows.h>
#include<utility>
#include<unordered_map>
#include<type_traits>
#include<string>
#include<filesystem>
#include<future>

#include"SV/Precompiled Global.hpp"
#include"Application/Application.hpp"
#include"SV/Utilities/SVUtilities.hpp"
#include"SV/OpenGL/VAO.hpp"
#include"SV/Utilities/Array.hpp"
#include"SV/Utilities/SVMemory.hpp"
#include"SV/Utilities/String.hpp"
#include"SV/OpenGL/ShaderProgram.hpp"
#include"SV/Utilities/Geometry.hpp"
#include"SV/Utilities/Chrono.hpp"
#include"SV/Utilities/Maths.hpp"
#include"SV/Utilities/Console.hpp"
#include"SV/Utilities/Tree.h"



using SV::Console;
using SV::ConsoleColour;

#define UpdateShaderFiles

/// <summary>
/// Entry point to space visuliser
/// </summary>
int main(int argc, const char* argv[])
{
	Console::SetConsole(GetStdHandle(STD_OUTPUT_HANDLE));

	Console::SetConsoleColour(ConsoleColour::White);
	Console::Write("Space Visuliser Refreshed V");
	Console::SetConsoleColour(ConsoleColour::BrightAqua);
	Console::Write("1");
	Console::SetConsoleColour(ConsoleColour::White);
	Console::Write(".");
	Console::SetConsoleColour(ConsoleColour::BrightBlue);
	Console::WriteLine("0");
	Console::SetConsoleColour(ConsoleColour::White);
	Console::Write("Architecture: ");
	Console::SetConsoleColour(ConsoleColour::BrightRed);
#ifdef _WIN64
	Console::WriteLine("x64");
#else
	Console::WriteLine("x86");
#endif // _WIN64

#ifdef UpdateShaderFiles
	Console::SetConsoleColour(ConsoleColour::BrightBlue);
	Console::NewLine();
	Console::WriteLine("Updating Shader Files...");
	SV::String command("call xcopy.exe \""_str + SOLUTION_DIR + "Space Visualiser\\Shaders\" \"" + OUTPUT_DIR + "Shaders\" /s /y /i /d");
	system(command.GetStringArray());
	Console::NewLines(2);
#endif // UpdateShaderFiles

	Console::SetConsoleColour(ConsoleColour::Yellow);
	Console::WriteLine("Yellow = WARNING; A event that displays a warning.");
	Console::SetConsoleColour(ConsoleColour::Red);
	Console::WriteLine("Red = ERROR; A event that displays a fatal error.If this occurs, the program will automatically exit and create a log.");
	Console::SetConsoleColour(ConsoleColour::Aqua);
	Console::WriteLine("Aqua = INITIALIZING; A event that displays a the program is initializing resources.");
	Console::SetConsoleColour(ConsoleColour::BrightBlue);
	Console::WriteLine("Blue = INFORMATION; A event that displaying important or useful information.");
	Console::SetConsoleColour(ConsoleColour::Green);
	Console::WriteLine("Green = SUCCESS; A event that displaying that an action has executed successfully.");
	Console::SetConsoleColour(ConsoleColour::Purple);
	Console::WriteLine("Purple = CLEARING; A event that displays a the program is clearing and removing resources.\r\n\r\n");

	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	Console::SetConsoleColour(ConsoleColour::BrightBlue);
	Console::WriteLine("CPU Information: ");
	Console::SetConsoleColour(ConsoleColour::BrightAqua);
	std::cout << "Processor Type: " << systemInfo.dwProcessorType << "\n";
	std::cout << "Processor Cores: " << systemInfo.dwNumberOfProcessors << "\n";
	std::cout << "Page Size: " << systemInfo.dwPageSize << "\n";
	Console::NewLines(2);

	if (!Application::Initilise(argc >= 1 ? argv[0] : nullptr)) return EXIT_FAILURE;

	return Application::Run();
}
