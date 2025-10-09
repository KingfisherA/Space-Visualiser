#include"SV-Engine/Precompiled Global.hpp"
#include"Console.hpp"
#include"String.hpp"
#include<iostream>

//
//
#ifdef _WIN32
HANDLE SV::Console::ConsoleHandle = nullptr;
#endif

//

#ifdef _WIN32
void SV::Console::SetConsole(HANDLE cHandle) { ConsoleHandle = cHandle; }
#endif

void SV::Console::SetConsoleColour(ConsoleColour colour) { 
	#ifdef _WIN32
		SetConsoleTextAttribute(ConsoleHandle, WORD(colour)); 
	#endif
}

//

#ifdef _WIN32
HANDLE SV::Console::GetConsoleHandle() { return ConsoleHandle; }

HWND SV::Console::GetConsoleHandleHWND() { return GetConsoleWindow(); }

#endif

//

void SV::Console::NewLine() { std::cout << '\n'; }

void SV::Console::NewLines(unsigned int numberOfLines)
{
	String newLines(numberOfLines, '\n');
	std::cout << newLines;
}

//

void SV::Console::WriteLine(const StringView& text) { std::cout << text << '\n'; }

void SV::Console::Write(const StringView& text) { std::cout << text; }

void SV::Console::WriteVector(const StringView& name, const glm::vec<4, float>& vector)
{
	SV::String textData = name + ", vec4(" + SV::NumberToString<float>(vector.x) + ", " + SV::NumberToString<float>(vector.y) + ", " + SV::NumberToString<float>(vector.z) + ", " + SV::NumberToString<float>(vector.w) + ')';
	std::cout << textData;
}

void SV::Console::WriteVectorLine(const StringView& name, const glm::vec<4, float>& vector)
{
	SV::String textData = name + ", vec4(" + SV::NumberToString<float>(vector.x) + ", " + SV::NumberToString<float>(vector.y) + ", " + SV::NumberToString<float>(vector.z) + ", " + SV::NumberToString<float>(vector.w) + ")\n";
	std::cout << textData;
}

int SV::Console::Pause()
{
	return system("PAUSE");
}

void SV::Console::Focus()
{
	#ifdef _WIN32
	SetForegroundWindow(GetConsoleWindow());
	#endif
}

void SV::Console::WriteVectorLine(const StringView& name, const glm::vec<3, float>& vector)
{
	SV::String textData = name + ", vec3(" + SV::NumberToString<float>(vector.x) + ", " + SV::NumberToString<float>(vector.y) + ", " + SV::NumberToString<float>(vector.z) + ")\n";
	std::cout << textData;
}

void SV::Console::WriteVectorLine(const StringView& name, const glm::vec<2, float>& vector)
{
	SV::String textData = name + ", vec2(" + SV::NumberToString<float>(vector.x) + ", " + SV::NumberToString<float>(vector.y) + ")\n";
	std::cout << textData;
}

void SV::Console::WriteVector(const StringView& name, const glm::vec<3, float>& vector)
{
	SV::String textData = name + ", vec3(" + SV::NumberToString<float>(vector.x) + ", " + SV::NumberToString<float>(vector.y) + ", " + SV::NumberToString<float>(vector.z) + ')';
	std::cout << textData;
}

void SV::Console::WriteVector(const StringView& name, const glm::vec<2, float>& vector)
{
	SV::String textData = name + ", vec2(" + SV::NumberToString<float>(vector.x) + ", " + SV::NumberToString<float>(vector.y) + ')';
	std::cout << textData;
}
