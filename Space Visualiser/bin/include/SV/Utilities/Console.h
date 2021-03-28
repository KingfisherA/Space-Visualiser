#pragma once
#include<glm.hpp>

typedef void* HANDLE;

namespace SV
{
	template<typename T>
	class BasicString;
	template<typename T>
	class BasicStringView;

	using StringView = BasicStringView<char>;
	using String= BasicString<char>;

	//
	enum class ConsoleColour : unsigned char
	{
		Black = 0,
		Blue = 1,
		Green = 2,
		Aqua = 3,
		Red = 4,
		Purple = 5,
		Yellow = 6,
		White = 7,
		Grey = 8,
		BrightBlue = 9,
		BrightGreen = 10,
		BrightAqua = 11,
		BrightRed = 12,
		BrightPurple = 13,
		BrightYellow = 14
	};

	//Class for handling console activities
	class Console abstract final
	{
	public:
		//
		static void SetConsole(HANDLE cHandle);
		static void SetConsoleColour(ConsoleColour colour);
		//
		static HANDLE GetConsoleHandle();
		static HWND GetConsoleHandleHWND();
		//
		static void NewLine();
		static void NewLines(unsigned int numberOfLines);
		//
		static void WriteLine(const StringView& text);
		static void Write(const StringView& text);
		static void WriteVector(const StringView& name, const glm::vec<2, float>& vector);
		static void WriteVector(const StringView& name, const glm::vec<3, float>& vector);
		static void WriteVector(const StringView& name, const glm::vec<4, float>& vector);
		static void WriteVectorLine(const StringView& name, const glm::vec<2, float>& vector);
		static void WriteVectorLine(const StringView& name, const glm::vec<3, float>& vector);
		static void WriteVectorLine(const StringView& name, const glm::vec<4, float>& vector);
		//
		static int Pause();
		static void Focus();
		//
	private:
		//
		static HANDLE ConsoleHandle;
	};

};