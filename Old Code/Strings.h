#pragma once
#include<corecrt_malloc.h>
#include<xutility>
#include<exception>
#include<string>
#include<iostream>
#include<memory>
#include"SV/Utilities/Array.h"
#include"SV/Utilities/Errors.h"
#include<xhash>

namespace SV
{
	class StringView;
	class String;
}

using stringview = SV::StringView;
using svstring = SV::String;
//
#pragma region Global Operators
//
SV::String operator""_sv(const char* stringA, std::size_t length);
SV::StringView operator""_sb(const char* stringA, std::size_t length);
//
SV::String operator+(const char* str1, const SV::StringView str2);
SV::String operator+(char* str1, const SV::StringView str2);
//
bool operator== (const char* str1, const SV::StringView str2);
bool operator== (char* str1, const SV::StringView str2);
//
bool operator!= (const char* str1, const SV::StringView str2);
bool operator!= (char* str1, const SV::StringView str2);
//
std::ostream& operator<<(std::ostream& out, const SV::StringView str);
std::istream& operator>>(std::istream& in, SV::String& str);
//
#pragma endregion
//

namespace SV
{
	using StringIndex = std::size_t;

	class StringView
	{
	public:
#pragma region Static Constants
		/// <summary>
/// Represents an empty string
/// </summary>
		static const StringView EmptyString;
		/// <summary>
		/// Represents a nullptr character
		/// </summary>
		static constexpr char StringNull = '\0';
		//
	protected:
		friend class SV::String;
		// A pointer to the nullptr-terminated string character array
		char* stringData;
		// The length of the string character array
		size_t length;
		//
	public:

		constexpr StringView() : stringData(nullptr), length(0) {}
		StringView(const char* str);
		StringView(const char* str, size_t lengthIn);
		StringView(const std::string& str);
		StringView(const SV::StringView& obj);
		//
		virtual ~StringView();

#pragma region Static String Operations

		/// <summary>
		/// Get the length of a generic c-string
		/// </summary>
		/// <param name="string">The string to retrieve the length from</param>
		/// <returns>The length of the string or StringUndefined if the length is greater than StringUndefined or if the length could not be found</returns>
		static constexpr size_t GetCStringLength(const char* string);
		//
		static bool Equals(const SV::StringView& str1, const SV::StringView& str2);
		//
		static bool NotEqual(const SV::StringView& str1, const SV::StringView& str2);
		/// <summary>
		/// Pre-calculate a Knuth-Morris-Pratt search table. Useful if the same pattern of charcters is searched multiple times in a string.
		/// </summary>
		/// <param name="pattern">The pattern to calculate the table from</param>
		/// <returns>The search table</returns>
		static SV::Array<size_t> CalculateFindTable(const SV::StringView& pattern);
		/// <summary>
		/// Pre-calculate a Knuth-Morris-Pratt search table. Useful if the same pattern of charcters is searched multiple times in a string.
		/// </summary>
		/// <param name="pattern">The pattern to calculate the table from</param>
		/// <param name="table">A pointer to the table to write into</param>
		/// <returns>The search table</returns>
		static void CalculateFindTable(const SV::StringView& pattern, SV::Array<size_t>& table);
		/// <summary>
		/// Perform a Knuth-Morris-Pratt string search
		/// </summary>
		/// <param name="searchString">The string to search</param>
		/// <param name="pattern">The pattern in the string to find</param>
		/// <param name="offset">(Optional Argument) The index to begin the search</param>
		/// <returns>The index where the pattern was found in the search string. StringUndefined retured if it does not exist or an error occured.</returns>
		static size_t Find(const SV::StringView& searchString, const SV::StringView& pattern, size_t offset = 0);
		/// <summary>
		/// Perform a Knuth-Morris-Pratt string search
		/// </summary>
		/// <param name="searchString">The string to search</param>
		/// <param name="pattern">The pattern in the string to find</param>
		/// <param name="table">The pre-calculated search table</param>
		/// <param name="offset">(Optional Argument) The index to begin the search</param>
		/// <returns>The index where the pattern was found in the search string. StringUndefined retured if it does not exist or an error occured.</returns>
		static size_t Find(const SV::StringView& searchString, const SV::StringView& pattern, SV::Array<size_t>& table, size_t offset = 0);
		/// <summary>
		/// Find every occurance of a pattern in a string and return the list of indicies
		/// </summary>
		/// <param name="searchString">The string to search</param>
		/// <param name="pattern">The pattern to find in the search string</param>
		/// <returns>A list of indicies where the pattern was found in the search string. StringUndefined retured if it does not exist or an error occured.</returns>
		static SV::Array<size_t> FindAll(const SV::StringView& searchString, const SV::StringView& pattern);
		/// <summary>
		/// Returns if a string exists in another string
		/// </summary>
		/// <param name="searchString">The string to find a pattern in</param>
		/// <param name="stringToFind">The string to find in the search string</param>
		static bool Contains(const SV::StringView& searchString, const SV::StringView& stringToFind);
		/// <summary>
		/// Retrieve a string of characters from a specified index and length from inside another string
		/// </summary>
		/// <param name="string">The string to extract a string from</param>
		/// <param name="index">The index of the beginning of the desired string</param>
		/// <param name="length">The length of characters to extract</param>
		/// <returns>A new extracted string from the input string</returns>
		static SV::StringView Substring(const StringView& stringIn, size_t index, size_t lengthIn);
		//

#pragma endregion

#pragma region Instantiated Operators
		/// <summary>
		/// Check if this string starts with a specific character
		/// </summary>
		/// <param name="c">The character to test</param>
		bool StartsWith(char c) const;
		/// <summary>
		/// Check if this string starts with a specific string
		/// </summary>
		/// <param name="str">The string to test</param>
		bool StartsWith(const SV::StringView& str) const;
		/// <summary>
		/// Check if this string ends with a specific character
		/// </summary>
		/// <param name="c">The character to test</param>
		bool EndsWith(char c) const;
		/// <summary>
		/// Check if this string ends with a specific string
		/// </summary>
		/// <param name="str">The string to test</param>
		bool EndsWith(const SV::StringView& str) const;
		/// <summary>
		/// Check for equality between this string and another string
		/// </summary>
		/// <param name="str1">The string to compare with this string</param>
		bool Equals(const SV::StringView& str1) const;
		/// <summary>
		/// Check for inequality between this string and another string
		/// </summary>
		/// <param name="str1">The string to compare with this string</param>
		bool NotEqual(const SV::StringView& str1) const;
		//
		size_t Find(const SV::StringView& pattern, size_t offset = 0) const;
		//
		size_t Find(const SV::StringView& pattern, SV::Array<size_t>& table, size_t offset = 0) const;
		//
		SV::Array<size_t> FindAll(const StringView& stringToFind) const;
		//
		bool Contains(const StringView& stringToFind) const;
		//
		SV::StringView Substring(size_t index, size_t lengthIn) const;
		//
		std::string ToStdString() const;
		//
#pragma endregion

#pragma region Array Access Functions

		//
		const char& operator[](size_t index) const;
		/// <summary>
		/// Get a specific character
		/// </summary>
		/// <param name="index">The index of the character to retriveve</param>
		const char& Get(size_t index) const;
		/// <summary>
		/// Get the first character in the string
		/// </summary>
		const char& First() const;
		/// <summary>
		/// Get the last character in the string
		/// </summary>
		const char& Last() const;

#pragma endregion

#pragma region Operator Overloads

		//
		SV::String operator+(const StringView& str1) const;
		//
		SV::String operator+(const char* str1) const;
		//
		bool operator==(const StringView& str1) const;
		//
		bool operator==(const char* str1) const;
		//
		bool operator!=(const StringView& str1) const;
		//
		bool operator!=(const char* str1) const;
		//
		void operator=(const StringView& obj);
		constexpr void operator=(const char* str);
		void operator=(const std::string& str);
		//
		explicit operator const char* () const;
		explicit operator std::string() const;
		//

#pragma endregion

#pragma region Miscellaneous Functions

				/// <summary>
		/// Get the length of this string
		/// </summary>
		size_t GetLength() const;
		//
		const char* GetStringArray() const;
		//
		SV::Array<char> GetNewStringArray() const;
		//
		char* begin();
		const char* begin() const;
		//
		char* end();
		const char* end() const;
		//
		bool IsValid() const;
		//
		void PrintToConsole() const;
		//

#pragma endregion

	};

	/// <summary>
	/// Represents text as an array of characters
	/// </summary>
	class String : public StringView
	{
	private:
		//
		bool shouldDelete;

#pragma region Memory Mangement Functions

		void Allocate(size_t size);
		//
		void Delete();

#pragma endregion

	public:
#pragma region Constructors & Destructors

		/// <summary>
		/// Create a new empty string
		/// </summary>
		String();
		/// <summary>
		/// Create a new string from a single character
		/// </summary>
		/// <param name="string">The character to construct this string from</param>
		String(char c);
		//
		String(size_t size);
		/// <summary>
		/// Create a new string from a c-string
		/// </summary>
		/// <param name="string">The c-string to construct this string from</param>
		String(const char* string);
		/// <summary>
		/// Create a new string from an std::string
		/// </summary>
		/// <param name="string">The std::string to construct this string from</param>
		String(const std::string& string);
		//
		String(const SV::StringView& string);
		/// <summary>
		/// Create a new string from an array of characters
		/// </summary>
		/// <param name="string">The array of characters to construct this string from</param>
		String(const SV::Array<char>& string);
		/// <summary>
		/// Copy from another String instance
		/// </summary>
		/// <param name="string">The instance to copy from</param>
		String(const String& string);
		//
		String(String&& string) noexcept;
		//
	private:
		//
		explicit String(char* stringPtr, size_t size, bool shouldDelete = false);
		explicit String(const char* stringPtr, size_t size, bool shouldDelete = false);
		//
	public:
		/// <summary>
		/// Disposal deconstructor. Deletes this string's resources
		/// </summary>
		virtual ~String();

#pragma endregion

#pragma region Static String Operations
		/// <summary>
		/// Concatenate two strings together
		/// </summary>
		/// <param name="str1">The first string to concatenate</param>
		/// <param name="str2">The second string to concatenate</param>
		/// <returns>A new string with the concatentaion of the two strings</returns>
		static String Concatenate(const StringView& str1, const StringView& str2);
		/// <summary>
		/// Append a string to the end of the first string
		/// </summary>
		/// <param name="str1">The string to append a string to</param>
		/// <param name="str2">The string to append to the first string</param>
		static void Append(String& str1, const StringView& str2);
		/// <summary>
/// Append a character to the end of the first string
/// </summary>
/// <param name="str1">The string to append a string to</param>
/// <param name="c">The character to append to the first string</param>
		static void Append(String& str1, const char c);
		/// <summary>
		/// Split a string up at a specified delimiter
		/// </summary>
		/// <param name="string">The string to split</param>
		/// <param name="delimiter">The string defining where the string should be split at</param>
		/// <returns>The array of split strings</returns>
		static SV::Array<String> Split(const StringView& stringIn, const StringView& delimiter);
		/// <summary>
		/// Retrieve the number of words in a string
		/// </summary>
		/// <param name="string">The string to locate the number of words in</param>
		static size_t GetWordCount(const StringView& stringIn);
		/// <summary>
		/// Get an array containing all the words in a specified string
		/// </summary>
		/// <param name="string">The string to get the words from</param>
		static  SV::Array<String> GetWords(const StringView& stringIn);
		/// <summary>
		/// Replace a sequence of characters in a string with another sequence of characters
		/// </summary>
		/// <param name="string">The string to perform the replace on</param>
		/// <param name="oldString">The string to replace</param>
		/// <param name="newString">The string to replace the old string with</param>
		/// <returns>A new string with the replacements made</returns>
		static  String Replace(const StringView& string, const StringView& oldString, const StringView& newString);
		/// <summary>
		/// Replace a sequence of characters in a string with another sequence of characters and assign it to the original string
		/// </summary>
		/// <param name="string">The string to perform the replace and assignment on</param>
		/// <param name="oldString">The string to replace</param>
		/// <param name="newString">The string to replace the old string with</param>
		static void ReplaceAssign(String& stringIn, const StringView& oldString, const StringView& newString);
		/// <summary>
		/// Remove a sequence of characters in a string
		/// </summary>
		/// <param name="string">The string to perform the removal on</param>
		/// <param name="oldString">The string to remove</param>
		/// <returns>A new string with the removals made</returns>
		static  String Remove(const StringView& string, const StringView& oldString);
		/// <summary>
		/// Remove a sequence of characters in a string and assign it to the original string
		/// </summary>
		/// <param name="string">The string to perform the removal and assignment on</param>
		/// <param name="oldString">The string to remove</param>
		static  void RemoveAssign(String& string, const StringView& oldString);
		/// <summary>
	/// Join an array of strings with a delimiter
	/// </summary>
	/// <param name="strings">Tha array of strings to join</param>
	/// <returns>A new string with the joins made</returns>
		static String Join(const SV::Array<StringView>& strings);
		/// <summary>
		/// Join an array of strings with a delimiter
		/// </summary>
		/// <param name="joinDelimiter">The string to join the array of strings with</param>
		/// <param name="strings">Tha array of strings to join</param>
		/// <returns>A new string with the joins made</returns>
		static String Join(const StringView& joinDelimiter, const SV::Array<StringView>& strings);
		/// <summary>
		/// Insert a string into another string at an index
		/// </summary>
		/// <param name="string">The string to perform insertions on</param>
		/// <param name="stringToInsert">The string to insert</param>
		/// <param name="index">The index to insert the string at</param>
		/// <returns>A new string with the insertions made</returns>
		static String Insert(const StringView& string, const StringView& stringToInsert, size_t index);
		/// <summary>
		/// Insert a string into another string at an index and assign it to the first string **Redo this**
		/// </summary>
		/// <param name="string">The string to perform insertions and assignment on</param>
		/// <param name="stringToInsert">The string to insert</param>
		/// <param name="index">The index to insert the string at</param>
		static void InsertAssign(String& string, const StringView& stringToInsert, size_t index);
		/// <summary>
		/// Remove a sequence of characters from a string at a certain index
		/// </summary>
		/// <param name="string">The string to perform splicing on</param>
		/// <param name="index">The index which defines where the sequence of characters to remove begins</param>
		/// <param name="length">The length of characters to remove after the index</param>
		/// <returns>A new string with splicing applied</returns>
		static String Splice(const StringView& string, size_t index, size_t length);
		/// <summary>
		/// Remove a sequence of characters from a string at a certain index and assign it to the primary string
		/// </summary>
		/// <param name="string">The string to perform splicing and assignment on</param>
		/// <param name="index">The index which defines where the sequence of characters to remove begins</param>
		/// <param name="length">The length of characters to remove after the index</param>
		static void SpliceAssign(String& string, size_t index, size_t length);
		/// <summary>
		/// Reverse a string
		/// </summary>
		/// <param name="string">The string to reverse</param>
		/// <returns>A new string which is the reverse of the input string</returns>
		static String Reverse(const StringView& string);
		/// <summary>
		/// Reverse a string and assign it to the string
		/// </summary>
		/// <param name="string">The string to reverse and assign</param>
		static void ReverseAssign(String& string);
		/// <summary>
		/// Convert a character to upper case
		/// </summary>
		/// <param name="input">The character to convert to upper case. Ignores and returns non-alphabetic characters</param>
		/// <returns>The character converted to upper case</returns>
		static char CharToUpper(char input)
		{
			switch (input)
			{
			case 'a':
				return 'A';
				break;

			case 'b':
				return 'B';
				break;

			case 'c':
				return 'C';
				break;

			case 'd':
				return 'D';
				break;

			case 'e':
				return 'E';
				break;

			case 'f':
				return 'F';
				break;

			case 'g':
				return 'G';
				break;

			case 'h':
				return 'H';
				break;

			case 'i':
				return 'I';
				break;

			case 'j':
				return 'J';
				break;

			case 'k':
				return 'K';
				break;

			case 'l':
				return 'L';
				break;

			case 'm':
				return 'M';
				break;

			case 'n':
				return 'N';
				break;

			case 'o':
				return 'O';
				break;

			case 'p':
				return 'P';
				break;

			case 'q':
				return 'Q';
				break;

			case 'r':
				return 'R';
				break;

			case 's':
				return 'S';
				break;

			case 't':
				return 'T';
				break;

			case 'u':
				return 'U';
				break;

			case 'v':
				return 'V';
				break;

			case 'w':
				return 'W';
				break;

			case 'x':
				return 'X';
				break;

			case 'y':
				return 'Y';
				break;

			case 'z':
				return 'Z';
				break;
			}
			return input;
		}
		/// <summary>
		/// Convert a string to upper case
		/// </summary>
		/// <param name="str">The string to convert</param>
		/// <returns>A new string converted to upper case</returns>
		static String ToUpper(const StringView& str);
		/// <summary>
		/// Convert a string to upper case and assign it to the string
		/// </summary>
		/// <param name="str">The string to convert and assign</param>
		static void ToUpperAssign(String& string);
		/// <summary>
		/// Convert a character to lower case
		/// </summary>
		/// <param name="input">The character to convert to lower case. Ignores and returns non-alphabetic characters</param>
		/// <returns>The character converted to lower case</returns>
		static char CharToLower(char input)
		{
			switch (input)
			{
			case 'A':
				return 'a';
				break;

			case 'B':
				return 'b';
				break;

			case 'C':
				return 'c';
				break;

			case 'D':
				return 'd';
				break;

			case 'E':
				return 'e';
				break;

			case 'F':
				return 'f';
				break;

			case 'G':
				return 'g';
				break;

			case 'H':
				return 'h';
				break;

			case 'I':
				return 'i';
				break;

			case 'J':
				return 'j';
				break;

			case 'K':
				return 'k';
				break;

			case 'L':
				return 'l';
				break;

			case 'M':
				return 'm';
				break;

			case 'N':
				return 'n';
				break;

			case 'O':
				return 'o';
				break;

			case 'P':
				return 'p';
				break;

			case 'Q':
				return 'q';
				break;

			case 'R':
				return 'r';
				break;

			case 'S':
				return 's';
				break;

			case 'T':
				return 't';
				break;

			case 'U':
				return 'u';
				break;

			case 'V':
				return 'v';
				break;

			case 'W':
				return 'w';
				break;

			case 'X':
				return 'x';
				break;

			case 'Y':
				return 'y';
				break;

			case 'Z':
				return 'z';
				break;
			}
			return input;
		}
		/// <summary>
		/// Convert a string to lower case
		/// </summary>
		/// <param name="str">The string to convert</param>
		/// <returns>A new string converted to lower case</returns>
		static String ToLower(const StringView& str);
		/// <summary>
		/// Convert a string to lower case and assign it to the string
		/// </summary>
		/// <param name="str">The string to convert and assign</param>
		static void ToLowerAssign(String& string);

#pragma endregion

#pragma region Instantiated String Operations
		/// <summary>
		/// Concatenate a string to this string and return it in a new string
		/// </summary>
		/// <param name="str1">The string to concatenate to this one</param>
		/// <returns>A new string with str1 concatenated with this string</returns>
		String Concatenate(const StringView& str1) const;
		//
		void Append(const StringView& str1);
		void Append(const char c);
		//
		SV::Array<String> Split(const StringView& delimiter) const;
		//
		size_t GetWordCount() const;
		//
		SV::Array<String> GetWords() const;
		//
		SV::String Replace(const StringView& oldString, const StringView& newString) const;
		//
		void ReplaceAssign(const StringView& oldString, const StringView& newString);
		//
		String Remove(const StringView& oldString) const;
		//
		void RemoveAssign(const StringView& oldString);
		//
		String Insert(const StringView& stringToInsert, size_t index) const;
		//
		void InsertAssign(const StringView& stringToInsert, size_t index);
		//
		String Splice(size_t index, size_t length) const;
		//
		void SpliceAssign(size_t index, size_t length);
		//
		String Reverse() const;
		//
		void ReverseAssign();
		//
		String ToUpper() const;
		//
		void ToUpperAssign();
		//
		String ToLower() const;
		//
		void ToLowerAssign();
		//

#pragma endregion

#pragma region Operator Overloads

		void operator=(const String& string);
		void operator=(const StringView string);
		void operator=(const char* string);
		void operator=(const std::string& string);
		void operator=(const std::stringstream& ss) noexcept;
		//
		void operator=(String&& string) noexcept;
		
		void operator=(std::string&& stringIn) noexcept;
		//
		void operator+=(const StringView str1);
		void operator+=(const char* str1);
		//
		explicit operator char* ();

#pragma endregion

#pragma region Miscellaneous Functions
		/// <summary>
		/// Clear the string
		/// </summary>
		void Clear();
		//
#pragma endregion

#pragma region Mutable Array Access Functions

		//
		char& operator[](size_t index);
		/// <summary>
		/// Get and optionally set a specific character
		/// </summary>
		/// <param name="index">The index of the character to retriveve</param>
		char& Get(size_t index);
		/// <summary>
		/// Set a specific character
		/// </summary>
		/// <param name="c">The character to set</param>
		/// <param name="index">The index of the character to set</param>
		void Set(size_t index, char c);
		/// <summary>
/// Get the first character in the string
/// </summary>
		char& First();
		/// <summary>
		/// Get the last character in the string
		/// </summary>
		char& Last();
		//

#pragma endregion

	};

};

namespace std
{
	template<> class hash<SV::StringView>
	{
	public:
		[[nodiscard]] size_t operator()(const SV::StringView& s) const noexcept
		{
			return  _Hash_array_representation(s.GetStringArray(), s.GetLength());
		}
	};
	template<> class hash<SV::String>
	{
	public:
		size_t operator()(const SV::String& s) const
		{
			return _Hash_array_representation(s.GetStringArray(), s.GetLength());
		}
	};
	template<> class hash<SV::StringView*>
	{
	public:
		[[nodiscard]] size_t operator()(const SV::StringView* s) const noexcept
		{
			return _Hash_array_representation(s->GetStringArray(), s->GetLength());
		}
	};
	template<> class hash<const SV::StringView*>
	{
	public:
		[[nodiscard]] size_t operator()(const SV::StringView* s) const noexcept
		{
			return _Hash_array_representation(s->GetStringArray(), s->GetLength());
		}
	};
	template<> class hash<SV::String*>
	{
	public:
		size_t operator()(const SV::String* s) const
		{
			return _Hash_array_representation(s->GetStringArray(), s->GetLength());
		}
	};
	template<> class hash<const SV::String*>
	{
	public:
		size_t operator()(const SV::String* s) const
		{
			return _Hash_array_representation(s->GetStringArray(), s->GetLength());
		}
	};
}