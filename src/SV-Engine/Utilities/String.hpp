#pragma once
#include<string>
#include<type_traits>
#include<initializer_list>
#include<utility>

#include"Array.hpp"
#include"SVUtilities.hpp"
#include<string>


namespace SV
{
	template<typename A>
	class Array;

	template<typename T>
	class BasicStringView
	{
	public:
		static_assert(SV::IsCharacter<T>, "BasicStringView cannot be instantiated on non-character types");

#pragma region Static Constants
		/// <summary>
		/// Represents an empty string
		/// </summary>
		static const BasicStringView<T> EmptyString;
		/// <summary>
		/// Represents a nullptr character
		/// </summary>
		static constexpr char StringNull = '\0';
#pragma endregion
		//
	protected:
		friend class SV::BasicString<T>;

		T charStore[2] = { 0 };
		// A pointer to the null-terminated string character array
		T* stringData = charStore;
		// The length of the string character array
		size_t stringLength = 0;
		//
	public:

		constexpr BasicStringView() { }
		constexpr BasicStringView(const T charIn)
		{
			charStore[0] = charIn;
			charStore[1] = 0;
			stringData = &charStore[0];
			stringLength = 1;
		}
		BasicStringView(const T* str);
		BasicStringView(const T* str, size_t lengthIn);
		BasicStringView(const std::initializer_list<T>& list);
		BasicStringView(const std::basic_string_view<T> stringView);
		BasicStringView(const std::basic_string<T>& str);
		BasicStringView(const BasicStringView<T>& obj);
		BasicStringView(const BasicString<T>& obj);
		//
		virtual ~BasicStringView() {}

#pragma region Static String Operations

		/// <summary>
		/// Get the length of a generic c-string
		/// </summary>
		/// <param name="string">The string to retrieve the length from</param>
		/// <returns>The length of the string or StringUndefined if the length is greater than StringUndefined or if the length could not be found</returns>
		static constexpr size_t GetCStringLength(const T* string);
		//
		static bool Equals(const BasicStringView<T>& str1, const BasicStringView<T>& str2);
		//
		static bool NotEqual(const BasicStringView<T>& str1, const BasicStringView<T>& str2);
		/// <summary>
		/// Pre-calculate a Knuth-Morris-Pratt search table. Useful if the same pattern of charcters is searched multiple times in a string.
		/// </summary>
		/// <param name="pattern">The pattern to calculate the table from</param>
		/// <returns>The search table</returns>
		static SV::Array<size_t> CalculateFindTable(const BasicStringView<T>& pattern);
		/// <summary>
		/// Pre-calculate a Knuth-Morris-Pratt search table. Useful if the same pattern of charcters is searched multiple times in a string.
		/// </summary>
		/// <param name="pattern">The pattern to calculate the table from</param>
		/// <param name="table">A pointer to the table to write into</param>
		/// <returns>The search table</returns>
		static void CalculateFindTable(const BasicStringView<T>& pattern, SV::Array<size_t>& table);
		/// <summary>
		/// Perform a Knuth-Morris-Pratt string search
		/// </summary>
		/// <param name="searchString">The string to search</param>
		/// <param name="pattern">The pattern in the string to find</param>
		/// <param name="offset">(Optional Argument) The index to begin the search</param>
		/// <returns>The index where the pattern was found in the search string. StringUndefined retured if it does not exist or an error occured.</returns>
		static size_t Find(const BasicStringView<T>& searchString, const BasicStringView<T>& pattern, size_t offset = 0);
		/// <summary>
		/// Perform a Knuth-Morris-Pratt string search
		/// </summary>
		/// <param name="searchString">The string to search</param>
		/// <param name="pattern">The pattern in the string to find</param>
		/// <param name="table">The pre-calculated search table</param>
		/// <param name="offset">(Optional Argument) The index to begin the search</param>
		/// <returns>The index where the pattern was found in the search string. StringUndefined retured if it does not exist or an error occured.</returns>
		static size_t Find(const BasicStringView<T>& searchString, const BasicStringView<T>& pattern, SV::Array<size_t>& table, size_t offset = 0);
		/// <summary>
		/// Find every occurance of a pattern in a string and return the list of indicies
		/// </summary>
		/// <param name="searchString">The string to search</param>
		/// <param name="pattern">The pattern to find in the search string</param>
		/// <returns>A list of indicies where the pattern was found in the search string. StringUndefined retured if it does not exist or an error occured.</returns>
		static SV::Array<size_t> FindAll(const BasicStringView<T>& searchString, const BasicStringView<T>& pattern);
		/// <summary>
		/// Returns if a string exists in another string
		/// </summary>
		/// <param name="searchString">The string to find a pattern in</param>
		/// <param name="stringToFind">The string to find in the search string</param>
		static bool Contains(const BasicStringView<T>& searchString, const BasicStringView<T>& stringToFind);
		/// <summary>
		/// Retrieve a string of characters from a specified index and length from inside another string
		/// </summary>
		/// <param name="string">The string to extract a string from</param>
		/// <param name="index">The index of the beginning of the desired string</param>
		/// <param name="length">The length of characters to extract</param>
		/// <returns>A new extracted string from the input string</returns>
		static SV::BasicStringView<T> Substring(const BasicStringView<T>& stringIn, size_t index, size_t lengthIn);
		/// <summary>
		/// Split a string up at a specified delimiter
		/// </summary>
		/// <param name="string">The string to split</param>
		/// <param name="delimiter">The string defining where the string should be split at</param>
		/// <returns>The array of split strings</returns>
		static SV::Array<BasicStringView<T>> SplitView(const BasicStringView<T>& stringIn, const BasicStringView<T>& delimiter);
		//
		static BasicStringView<T> TrimWhitespaces(const BasicStringView<T>& stringIn);
		//

		/// <summary>
		/// Convert a character to upper case
		/// </summary>
		/// <param name="input">The character to convert to upper case. Ignores and returns non-alphabetic characters</param>
		/// <returns>The character converted to upper case</returns>
		template<typename CharType>
		static constexpr CharType CharToUpper(const CharType input)
		{
			static_assert(SV::IsCharacter<CharType>, "Template parameter \"CharType\" must be a valid character type");
			switch (input)
			{
			case (CharType)'a':
				return (CharType)'A';
				 

			case (CharType)'b':
				return (CharType)'B';
				 

			case (CharType)'c':
				return (CharType)'C';
				 

			case (CharType)'d':
				return (CharType)'D';
				 

			case (CharType)'e':
				return (CharType)'E';
				 

			case (CharType)'f':
				return (CharType)'F';
				 

			case (CharType)'g':
				return (CharType)'G';
				 

			case (CharType)'h':
				return (CharType)'H';
				 

			case (CharType)'i':
				return (CharType)'I';
				 

			case (CharType)'j':
				return (CharType)'J';
				 

			case (CharType)'k':
				return (CharType)'K';
				 

			case (CharType)'l':
				return (CharType)'L';
				 

			case (CharType)'m':
				return (CharType)'M';
				 

			case (CharType)'n':
				return (CharType)'N';
				 

			case (CharType)'o':
				return (CharType)'O';
				 

			case (CharType)'p':
				return (CharType)'P';
				 

			case (CharType)'q':
				return (CharType)'Q';
				 

			case (CharType)'r':
				return (CharType)'R';
				 

			case (CharType)'s':
				return (CharType)'S';
				 

			case (CharType)'t':
				return (CharType)'T';
				 

			case (CharType)'u':
				return (CharType)'U';
				 

			case (CharType)'v':
				return (CharType)'V';
				 

			case (CharType)'w':
				return (CharType)'W';
				 

			case (CharType)'x':
				return (CharType)'X';
				 

			case (CharType)'y':
				return (CharType)'Y';
				 

			case (CharType)'z':
				return (CharType)'Z';
				 

			default:
				return (CharType)input;
			}
		}

		/// <summary>
		/// Convert a character to lower case
		/// </summary>
		/// <param name="input">The character to convert to lower case. Ignores and returns non-alphabetic characters</param>
		/// <returns>The character converted to lower case</returns>
		template<typename CharType>
		static constexpr CharType CharToLower(const CharType input)
		{
			static_assert(SV::IsCharacter<CharType>, "Template parameter \"CharType\" must be a valid character type");
			switch (input)
			{
			case (CharType)'A':
				return (CharType)'a';


			case (CharType)'B':
				return (CharType)'b';


			case (CharType)'C':
				return (CharType)'c';


			case (CharType)'D':
				return (CharType)'d';


			case (CharType)'E':
				return (CharType)'e';


			case (CharType)'F':
				return (CharType)'f';


			case (CharType)'G':
				return (CharType)'g';


			case (CharType)'H':
				return (CharType)'h';


			case (CharType)'I':
				return (CharType)'i';


			case (CharType)'J':
				return (CharType)'j';


			case (CharType)'K':
				return (CharType)'k';


			case (CharType)'L':
				return (CharType)'l';


			case (CharType)'M':
				return (CharType)'m';


			case (CharType)'N':
				return (CharType)'n';


			case (CharType)'O':
				return (CharType)'o';


			case (CharType)'P':
				return (CharType)'p';


			case (CharType)'Q':
				return (CharType)'q';


			case (CharType)'R':
				return (CharType)'r';


			case (CharType)'S':
				return (CharType)'s';


			case (CharType)'T':
				return (CharType)'t';


			case (CharType)'U':
				return (CharType)'u';


			case (CharType)'V':
				return (CharType)'v';


			case (CharType)'W':
				return (CharType)'w';


			case (CharType)'X':
				return (CharType)'x';


			case (CharType)'Y':
				return (CharType)'y';


			case (CharType)'Z':
				return (CharType)'z';


			default:
				return (CharType)input;
			}
		}

#pragma endregion

#pragma region Instantiated Operators
		/// <summary>
		/// Check if this string starts with a specific character
		/// </summary>
		/// <param name="c">The character to test</param>
		bool StartsWith(T c) const;
		/// <summary>
		/// Check if this string starts with a specific string
		/// </summary>
		/// <param name="str">The string to test</param>
		bool StartsWith(const BasicStringView<T>& str) const;
		/// <summary>
		/// Check if this string ends with a specific character
		/// </summary>
		/// <param name="c">The character to test</param>
		bool EndsWith(T c) const;
		/// <summary>
		/// Check if this string ends with a specific string
		/// </summary>
		/// <param name="str">The string to test</param>
		bool EndsWith(const BasicStringView<T>& str) const;
		/// <summary>
		/// Check for equality between this string and another string
		/// </summary>
		/// <param name="str1">The string to compare with this string</param>
		bool Equals(const BasicStringView<T>& str1) const;
		/// <summary>
		/// Check for inequality between this string and another string
		/// </summary>
		/// <param name="str1">The string to compare with this string</param>
		bool NotEqual(const BasicStringView<T>& str1) const;
		//
		size_t Find(const BasicStringView<T>& pattern, size_t offset = 0) const;
		//
		size_t Find(const BasicStringView<T>& pattern, SV::Array<size_t>& table, size_t offset = 0) const;
		//
		SV::Array<size_t> FindAll(const BasicStringView<T>& stringToFind) const;
		//
		bool Contains(const BasicStringView<T>& stringToFind) const;
		//
		SV::BasicStringView<T> Substring(size_t index, size_t lengthIn) const;
		//
		SV::Array<BasicStringView<T>> SplitView(const BasicStringView<T>& delimeter) const;
		//
		SV::BasicStringView<T> TrimWhitespaces() const;
		//
		std::basic_string<T> ToStdString() const;
		//
#pragma endregion

#pragma region Array Access Functions

		//
		const T& operator[](size_t index) const;
		/// <summary>
		/// Get a specific character
		/// </summary>
		/// <param name="index">The index of the character to retriveve</param>
		const T& Get(size_t index) const;
		/// <summary>
		/// Get the first character in the string
		/// </summary>
		const T& First() const;
		/// <summary>
		/// Get the last character in the string
		/// </summary>
		const T& Last() const;

#pragma endregion

#pragma region Operator Overloads
		//
		bool operator==(const BasicStringView<T>& str1) const;
		bool operator==(const T* str1) const;
		//
		bool operator!=(const BasicStringView<T>& str1) const;
		bool operator!=(const T* str1) const;
		//
		void operator=(const BasicStringView<T>& obj);
		constexpr void operator=(const T* str);
		void operator=(const std::basic_string<T>& str);
		//
		explicit operator const T* () const;
		explicit operator std::basic_string_view<T>() const
		{
			return std::basic_string_view<T>(this->stringData, this->stringLength);
		}
		explicit operator std::basic_string<T>() const
		{
			return std::basic_string<T>(this->stringData, this->stringLength);
		}
		//
		inline std::basic_string<T> CopyToStdString() const
		{
			return std::basic_string<T>(this->stringData, this->stringLength);
		}

#pragma endregion

#pragma region Miscellaneous Functions

		/// <summary>
		/// Get the length of this string
		/// </summary>
		size_t GetLength() const;
		//
		const T* GetStringArray() const;
		//
		SV::Array<T> GetNewStringArray() const;
		//
		const T* begin() const;
		const T* end() const;
		//
		bool IsEmpty() const;
		//
		void PrintToConsole() const;
		//
		size_t Hash() const;
		//

#pragma endregion

	};

	constexpr size_t STR_OPTIMISE_STORE = 11;

	template<typename T>
	class BasicString : public BasicStringView<T>
	{
	private:

		enum class AllocatorStore : unsigned char
		{
			Heap,
			SSOptimise
		};

		AllocatorStore allocatorStore = AllocatorStore::Heap;
		T optimiseStore[STR_OPTIMISE_STORE] = { 0 };

		void Allocate(std::size_t elements);

		size_t stringCapacity = 0;

	public:
		using MyStringView = SV::BasicStringView<T>;
		static_assert(SV::IsCharacter<T>, "BasicString cannot be instantiated on non-character types");

#pragma region Constructors & Destructors

		/// <summary>
		/// Create a new empty string
		/// </summary>
		BasicString();
		/// <summary>
		/// Create a new string from a single character
		/// </summary>
		/// <param name="string">The character to construct this string from</param>
		BasicString(T c);
		//
		BasicString(size_t size, T fillerChar = (T)0);
		/// <summary>
		/// Create a new string from a c-string
		/// </summary>
		/// <param name="string">The c-string to construct this string from</param>
		BasicString(const T* string);
		/// <summary>
		/// Create a new string from an std::string
		/// </summary>
		/// <param name="string">The std::string to construct this string from</param>
		BasicString(const std::string& string);
		BasicString(std::basic_string_view<T> stringView);
		BasicString(const std::initializer_list<T>& listIn);
		//
		BasicString(const BasicStringView<T>& string);
		/// <summary>
		/// Create a new string from an array of characters
		/// </summary>
		/// <param name="string">The array of characters to construct this string from</param>
		BasicString(const SV::Array<T>& string);
		/// <summary>
		/// Copy from another String instance
		/// </summary>
		/// <param name="string">The instance to copy from</param>
		BasicString(const SV::BasicString<T>& string);
		//
		BasicString(SV::BasicString<T>&& string) noexcept;
		//
		explicit BasicString(const T* stringPtr, size_t size);
		//
		virtual ~BasicString() noexcept;
		//
#pragma endregion

#pragma region Static String Operations
		/// <summary>
		/// Concatenate two strings together
		/// </summary>
		/// <param name="str1">The first string to concatenate</param>
		/// <param name="str2">The second string to concatenate</param>
		/// <returns>A new string with the concatentaion of the two strings</returns>
		static BasicString<T> Concatenate(const BasicStringView<T>& str1, const BasicStringView<T>& str2);
		/// <summary>
		/// Append a string to the end of the first string
		/// </summary>
		/// <param name="str1">The string to append a string to</param>
		/// <param name="str2">The string to append to the first string</param>
		static void Append(BasicString<T>& str1, const BasicStringView<T>& str2);
		/// <summary>
/// Append a character to the end of the first string
/// </summary>
/// <param name="str1">The string to append a string to</param>
/// <param name="c">The character to append to the first string</param>
		static void Append(BasicString<T>& str1, const char c);
		/// <summary>
		/// Split a string up at a specified delimiter
		/// </summary>
		/// <param name="string">The string to split</param>
		/// <param name="delimiter">The string defining where the string should be split at</param>
		/// <returns>The array of split strings</returns>
		static SV::Array<BasicString<T>> Split(const BasicStringView<T>& stringIn, const BasicStringView<T>& delimiter);
		/// <summary>
		/// Retrieve the number of words in a string
		/// </summary>
		/// <param name="string">The string to locate the number of words in</param>
		static size_t GetWordCount(const BasicStringView<T>& stringIn);
		/// <summary>
		/// Get an array containing all the words in a specified string
		/// </summary>
		/// <param name="string">The string to get the words from</param>
		static  SV::Array<BasicString<T>> GetWords(const BasicStringView<T>& stringIn);
		/// <summary>
		/// Replace a sequence of characters in a string with another sequence of characters
		/// </summary>
		/// <param name="string">The string to perform the replace on</param>
		/// <param name="oldString">The string to replace</param>
		/// <param name="newString">The string to replace the old string with</param>
		/// <returns>A new string with the replacements made</returns>
		static  BasicString<T> Replace(const BasicStringView<T>& string, const BasicStringView<T>& oldString, const BasicStringView<T>& newString);
		/// <summary>
		/// Replace a sequence of characters in a string with another sequence of characters and assign it to the original string
		/// </summary>
		/// <param name="string">The string to perform the replace and assignment on</param>
		/// <param name="oldString">The string to replace</param>
		/// <param name="newString">The string to replace the old string with</param>
		static void ReplaceAssign(BasicString<T>& stringIn, const BasicStringView<T>& oldString, const BasicStringView<T>& newString);
		/// <summary>
		/// Remove a sequence of characters in a string
		/// </summary>
		/// <param name="string">The string to perform the removal on</param>
		/// <param name="oldString">The string to remove</param>
		/// <returns>A new string with the removals made</returns>
		static  BasicString<T> Remove(const BasicStringView<T>& string, const BasicStringView<T>& oldString);
		/// <summary>
		/// Remove a sequence of characters in a string and assign it to the original string
		/// </summary>
		/// <param name="string">The string to perform the removal and assignment on</param>
		/// <param name="oldString">The string to remove</param>
		static  void RemoveAssign(BasicString<T>& string, const BasicStringView<T>& oldString);
		/// <summary>
	/// Join an array of strings with a delimiter
	/// </summary>
	/// <param name="strings">Tha array of strings to join</param>
	/// <returns>A new string with the joins made</returns>
		static BasicString<T> Join(const SV::Array<BasicStringView<T>>& strings);
		/// <summary>
		/// Join an array of strings with a delimiter
		/// </summary>
		/// <param name="joinDelimiter">The string to join the array of strings with</param>
		/// <param name="strings">Tha array of strings to join</param>
		/// <returns>A new string with the joins made</returns>
		static BasicString<T> Join(const BasicStringView<T>& joinDelimiter, const SV::Array<BasicStringView<T>>& strings);
		/// <summary>
		/// Insert a string into another string at an index
		/// </summary>
		/// <param name="string">The string to perform insertions on</param>
		/// <param name="stringToInsert">The string to insert</param>
		/// <param name="index">The index to insert the string at</param>
		/// <returns>A new string with the insertions made</returns>
		static BasicString<T> Insert(const BasicStringView<T>& string, const BasicStringView<T>& stringToInsert, size_t index);
		/// <summary>
		/// Insert a string into another string at an index and assign it to the first string **Redo this**
		/// </summary>
		/// <param name="string">The string to perform insertions and assignment on</param>
		/// <param name="stringToInsert">The string to insert</param>
		/// <param name="index">The index to insert the string at</param>
		static void InsertAssign(BasicString<T>& string, const BasicStringView<T>& stringToInsert, size_t index);
		/// <summary>
		/// Remove a sequence of characters from a string at a certain index
		/// </summary>
		/// <param name="string">The string to perform splicing on</param>
		/// <param name="index">The index which defines where the sequence of characters to remove begins</param>
		/// <param name="length">The length of characters to remove after the index</param>
		/// <returns>A new string with splicing applied</returns>
		static BasicString<T> Splice(const BasicStringView<T>& string, size_t index, size_t length);
		/// <summary>
		/// Remove a sequence of characters from a string at a certain index and assign it to the primary string
		/// </summary>
		/// <param name="string">The string to perform splicing and assignment on</param>
		/// <param name="index">The index which defines where the sequence of characters to remove begins</param>
		/// <param name="length">The length of characters to remove after the index</param>
		static void SpliceAssign(BasicString<T>& string, size_t index, size_t length);
		/// <summary>
		/// Reverse a string
		/// </summary>
		/// <param name="string">The string to reverse</param>
		/// <returns>A new string which is the reverse of the input string</returns>
		static BasicString<T> Reverse(const BasicStringView<T>& string);
		/// <summary>
		/// Reverse a string and assign it to the string
		/// </summary>
		/// <param name="string">The string to reverse and assign</param>
		static void ReverseAssign(BasicString<T>& string);
		/// <summary>
		/// Convert a string to upper case
		/// </summary>
		/// <param name="str">The string to convert</param>
		/// <returns>A new string converted to upper case</returns>
		static BasicString<T> ToUpper(const BasicStringView<T>& str);
		/// <summary>
		/// Convert a string to upper case and assign it to the string
		/// </summary>
		/// <param name="str">The string to convert and assign</param>
		static void ToUpperAssign(BasicString<T>& string);
		/// <summary>
		/// Convert a string to lower case
		/// </summary>
		/// <param name="str">The string to convert</param>
		/// <returns>A new string converted to lower case</returns>
		static BasicString<T> ToLower(const BasicStringView<T>& str);
		/// <summary>
		/// Convert a string to lower case and assign it to the string
		/// </summary>
		/// <param name="str">The string to convert and assign</param>
		static void ToLowerAssign(BasicString<T>& string);

#pragma endregion

#pragma region Instantiated String Operations
		/// <summary>
		/// Concatenate a string to this string and return it in a new string
		/// </summary>
		/// <param name="str1">The string to concatenate to this one</param>
		/// <returns>A new string with str1 concatenated with this string</returns>
		BasicString<T> Concatenate(const BasicStringView<T>& str1) const;
		//
		void Append(const BasicStringView<T>& str1);
		void Append(const char c);
		//
		SV::Array<BasicString<T>> Split(const BasicStringView<T>& delimiter) const;
		//
		size_t GetWordCount() const;
		//
		SV::Array<BasicString<T>> GetWords() const;
		//
		BasicString<T> Replace(const BasicStringView<T>& oldString, const BasicStringView<T>& newString) const;
		//
		void ReplaceAssign(const BasicStringView<T>& oldString, const BasicStringView<T>& newString);
		//
		BasicString<T> Remove(const BasicStringView<T>& oldString) const;
		//
		void RemoveAssign(const BasicStringView<T>& oldString);
		//
		BasicString<T> Insert(const BasicStringView<T>& stringToInsert, size_t index) const;
		//
		void InsertAssign(const BasicStringView<T>& stringToInsert, size_t index);
		//
		BasicString<T> Splice(size_t index, size_t length) const;
		//
		void SpliceAssign(size_t index, size_t length);
		//
		BasicString<T> Reverse() const;
		//
		void ReverseAssign();
		//
		BasicString<T> ToUpper() const;
		//
		void ToUpperAssign();
		//
		BasicString<T> ToLower() const;
		//
		void ToLowerAssign();
		//

#pragma endregion

#pragma region Operator Overloads

		BasicString<T>& operator=(const BasicString<T>& string);
		BasicString<T>& operator=(const BasicStringView<T>& string);
		BasicString<T>& operator=(const T* string);
		BasicString<T>& operator=(const std::basic_string<T>& string);
		BasicString<T>& operator=(const std::basic_stringstream<T>& ss) noexcept;
		//
		BasicString<T>& operator=(BasicString<T>&& string) noexcept;
		//
		BasicString<T>& operator+=(const BasicStringView<T>& str1);
		BasicString<T>& operator+=(const BasicString<T>& str1);
		BasicString<T>& operator+=(const T* str1);
		//
		explicit operator T* ();

#pragma endregion

#pragma region Miscellaneous Functions
		/// <summary>
		/// Clear the string
		/// </summary>
		void Clear(const bool freeMemory = true);
		//
		void TrimToCapacity();
		//
		size_t GetCapacity() const { return this->stringCapacity; }
		//
#pragma endregion

#pragma region Mutable Array Access Functions

		//
		T& operator[](size_t index);
		/// <summary>
		/// Get and optionally set a specific character
		/// </summary>
		/// <param name="index">The index of the character to retriveve</param>
		T& Get(size_t index);
		/// <summary>
		/// Set a specific character
		/// </summary>
		/// <param name="c">The character to set</param>
		/// <param name="index">The index of the character to set</param>
		void Set(size_t index, T c);
		/// <summary>
/// Get the first character in the string
/// </summary>
		T& First();
		/// <summary>
		/// Get the last character in the string
		/// </summary>
		T& Last();
		//
		inline T* begin() { return &this->stringData[0]; }
		inline T* end() { return &this->stringData[this->stringLength]; }
		//

#pragma endregion

	};

#pragma region Aliases

	using StringView = BasicStringView<char>;
	using String = BasicString<char>;

	using StringViewW = BasicStringView<wchar_t>;
	using StringW = BasicString<wchar_t>;

#ifdef __cpp_char8_t
	using StringViewU8 = BasicStringView<char8_t>;
	using StringU8 = BasicString<char8_t>;
#endif

	// using StringViewU16 = BasicStringView<char16_t>;
	// using StringU16 = BasicString<char16_t>;

	// using StringViewU32 = BasicStringView<char32_t>;
	// using StringU32 = BasicString<char32_t>;

#pragma endregion

#pragma region Global String Operators Definitions
	//
	template<typename T>
	inline SV::BasicString<T> operator+(const T* str1, const BasicStringView<T>& str2)
	{
		return SV::BasicString<T>::Concatenate(str1, str2);
	}
	template<typename T>
	inline SV::BasicString<T> operator+(const T charIn, const BasicStringView<T>& strIn)
	{
		return SV::BasicString<T>::Concatenate(charIn, strIn);
	}
	template<typename T>
	inline SV::BasicString<T> operator+(const BasicStringView<T>& str1, const T* str2)
	{
		return SV::BasicString<T>::Concatenate(str1, str2);
	}
	template<typename T>
	inline SV::BasicString<T> operator+(const BasicStringView<T>& strIn, const T charIn)
	{
		return SV::BasicString<T>::Concatenate(strIn, charIn);
	}
	template<typename T>
	inline SV::BasicString<T> operator+(const BasicStringView<T>& str1, const BasicStringView<T>& str2)
	{
		return SV::BasicString<T>::Concatenate(str1, str2);
	}
	//
	template<typename T>
	inline bool operator== (const T* str1, const BasicStringView<T>& str2)
	{
		return SV::BasicStringView<T>::Equals(str1, str2);
	}
	template<typename T>
	inline bool operator==(const T* str1, const SV::BasicString<T>& str2)
	{
		return SV::BasicStringView<T>::Equals(str1, str2);
	}
	//
	template<typename T>
	inline bool operator!= (const T* str1, const BasicStringView<T>& str2)
	{
		return SV::BasicStringView<T>::NotEqual(str1, str2);
	}
	template<typename T>
	inline bool operator!=(const T* str1, const SV::BasicString<T>& str2)
	{
		return SV::BasicStringView<T>::NotEqual(str1, str2);
	}
	//
	template<typename T>
	inline std::ostream& operator<<(std::ostream& out, const BasicStringView<T>& str)
	{
		return out << std::basic_string_view<T>(str.GetStringArray(), str.GetLength());
	}
	//
	inline std::istream& operator>>(std::istream& in, SV::BasicString<char>& str)
	{
		char currentChar = 0;
		while (std::cin.good())
		{
			std::cin.read(&currentChar, 1);
			if (currentChar == '\n' || currentChar == '\r') break;

			str.Append(currentChar);
		}

		return in;
	}
	//
#pragma endregion

	template<typename Integral, typename T = char>
	BasicString<T> IntegralToString(Integral value) noexcept
	{
		static_assert(std::is_integral_v<Integral>, "Type \"Integral\" must be an integral type");

		T charBuffer[20];
		T* nextChar = &charBuffer[20];

		auto ConvertUIntToStr = [](T*& nextChar, auto integralValue) {
			do
			{
				*--nextChar = T('0') + T(integralValue % 10);
				integralValue /= 10;
			} while (integralValue != 0);
		};

		if constexpr (std::is_signed_v<Integral>)
		{
			if (value < 0)
			{
				ConvertUIntToStr(nextChar, -value);
				*--nextChar = (T)'-';
			}
			else
			{
				ConvertUIntToStr(nextChar, value);
			}
		}
		else
		{
			ConvertUIntToStr(nextChar, value);
		}

		return BasicString<T>(nextChar, size_t(&charBuffer[20] - nextChar));
	}
	template<typename FloatingPointType, typename T = char>
	BasicString<T> FloatingPointToString(FloatingPointType value)
	{
		static_assert(std::is_floating_point_v<FloatingPointType>, "Template parameter \"FloatingPointNumber\" is not a floating point type.");

		return std::to_string(value).c_str();

		// const size_t strLength = static_cast<size_t>(sprintf(nullptr, "%f", (double)value));
		// BasicString<T> returnString(strLength);
		// sprintf(&returnString[0], "%f", (double)value);
		//return std::move(returnString);
	}

	template<typename Number, typename T = char>
	BasicString<T> NumberToString(Number value)
	{
		static_assert(std::is_integral_v<Number> || std::is_floating_point_v<Number>, "Template parameter \"Number\" is not a number type.");

		if constexpr (std::is_integral_v<Number>)
		{
			return std::move(SV::IntegralToString<Number, T>(value));
		}
		else
		{
			return std::move(SV::FloatingPointToString<Number, T>(value));
		}
	}

	/*
template<typename Number, typename T = char>
Number CharToNumber(const T charIn)
{
	static_assert(std::is_integral_v<Number> || std::is_floating_point_v<Number>, "Template parameter \"Number\" is not an number type.");
	//static_assert(std::is_char<Integral>, "Template parameter \"Integral\" is not an integral type.");

	switch (charIn)
	{
	case (T)'0':
		return 0;
	case (T)'1':
		return 1;
	case (T)'2':
		return 2;
	case (T)'3':
		return 3;
	case (T)'4':
		return 4;
	case (T)'5':
		return 5;
	case (T)'6':
		return 6;
	case (T)'7':
		return 7;
	case (T)'8':
		return 8;
	case (T)'9':
		return 9;
	default:
		throw std::runtime_error("Character inputted is not numerical.");
	}
}

template<typename Integral, typename T = char>
Integral StringToIntegral(const BasicStringView<T>& stringIn)
{
	static_assert(std::is_integral_v<Integral>, "Template parameter \"Integral\" is not an integral type.");

	SV::BasicStringView<T> steralisedString = stringIn.TrimWhitespaces();
	Integral result = 0;
	size_t i = 0;

	if (steralisedString[0] == (T)'-') i = 1;

	for (; i < steralisedString.GetLength(); i++)
	{
		result += CharToNumber<Integral, T>(steralisedString[i]) * Maths::IntegerPow((Integral)10, (steralisedString.GetLength() - 1 - i));
	}

	if (steralisedString[0] == (T)'-' && std::is_signed_v<Integral>) result *= -1;

	return result;
} */

	enum class IntegerBase : unsigned char
	{
		Automatic = 0,
		Binary = 2,
		Denary = 10,
		Hexadecimal = 16
	};

	template<typename Integral, typename T = char>
	Integral StringToIntegral(const BasicStringView<std::decay_t<T>>& stringIn, IntegerBase base = IntegerBase::Automatic)
	{
		static_assert(std::is_integral_v<Integral>, "Template parameter \"Integral\" is not an integral type.");

		char* buffer = (char*)alloca(stringIn.GetLength() + 1);

		if (buffer == nullptr) throw std::runtime_error("Cannot allocate memory for temporary buffer.");

		for (size_t i = 0; i < stringIn.GetLength(); i++)
		{
			buffer[i] = (char)stringIn[i];
		}
		buffer[stringIn.GetLength()] = 0;

		Integral result;
		if constexpr (std::is_signed_v<Integral>)
		{
			result = strtoll(buffer, nullptr, (int)base);
		}
		else
		{
			result = strtoull(buffer, nullptr, (int)base);
		}

		return result;
	}

	template<typename FloatingPointType, typename T = char>
	FloatingPointType StringToFloatingPoint(const BasicStringView<std::decay_t<T>>& stringIn) noexcept(false)
	{
		static_assert(std::is_floating_point_v<FloatingPointType>, "Template parameter \"FloatingPointType\" is not a floating point type.");

		char* buffer = (char*)alloca(stringIn.GetLength() + 1);

		if (buffer == nullptr) throw std::runtime_error("Cannot allocate memory for temporary buffer.");

		for (size_t i = 0; i < stringIn.GetLength(); i++)
		{
			buffer[i] = (char)stringIn[i];
		}
		buffer[stringIn.GetLength()] = 0;

		long double result = strtold(buffer, nullptr);

		return (FloatingPointType)result;
	}

	template<typename Number, typename T = char>
	Number StringToNumber(const BasicStringView<std::decay_t<T>>& stringIn)
	{
		static_assert(std::is_integral_v<Number> || std::is_floating_point_v<Number>, "Template parameter \"Number\" is not a number type.");

		if constexpr (std::is_floating_point_v< Number>)
		{
			return StringToFloatingPoint<Number, T>(stringIn);
		}
		else
		{
			return StringToIntegral<Number, T>(stringIn);
		}
	}

	template<typename Number>
	inline SV::String VecToString(const glm::vec<2, Number>& vec)
	{
		return "vec2(" + SV::NumberToString<Number>(vec.x) + "," + SV::NumberToString<Number>(vec.y) + ")";
	}
	template<typename Number>
	inline SV::String VecToString(const glm::vec<3, Number>& vec)
	{
		return "vec3(" + SV::NumberToString<Number>(vec.x) + "," + SV::NumberToString<Number>(vec.y) + "," + SV::NumberToString<Number>(vec.z) + ")";
	}
	template<typename Number>
	inline SV::String VecToString(const glm::vec<4, Number>& vec)
	{
		return "vec4(" + SV::NumberToString<Number>(vec.x) + "," + SV::NumberToString<Number>(vec.y) + "," + SV::NumberToString<Number>(vec.z) + "," + SV::NumberToString<Number>(vec.w) + ")";
	}
}

//
inline SV::String operator""_str(const char* stringA, std::size_t length)
{
	return SV::String(stringA, length);
}
inline SV::StringW operator""_str(const wchar_t* stringA, std::size_t length)
{
	return SV::StringW(stringA, length);
}
// inline SV::StringU16 operator""_str(const char16_t* stringA, std::size_t length)
// {
// 	return SV::StringU16(stringA, length);
// }
// inline SV::StringU32 operator""_str(const char32_t* stringA, std::size_t length)
// {
// 	return SV::StringU32(stringA, length);
// }
//
inline SV::StringView operator""_view(const char* stringA, std::size_t length)
{
	return SV::StringView(stringA, length);
}
inline SV::StringViewW operator""_view(const wchar_t* stringA, std::size_t length)
{
	return SV::StringViewW(stringA, length);
}
// inline SV::StringViewU16 operator""_view(const char16_t* stringA, std::size_t length)
// {
// 	return SV::StringViewU16(stringA, length);
// }
// inline SV::StringViewU32 operator""_view(const char32_t* stringA, std::size_t length)
// {
// 	return SV::StringViewU32(stringA, length);
// }
//

//Custom hash definitions for use with std::unordered_map
namespace std
{
	template<> class hash<SV::BasicStringView<char>>
	{
	public:
		[[nodiscard]] size_t operator()(const SV::BasicStringView<char> s) const noexcept
		{
			return std::hash<const char*>()(s.GetStringArray());
		}
	};
	template<> class hash<SV::BasicString<char>>
	{
	public:
		size_t operator()(const SV::BasicString<char>& s) const
		{
			return std::hash<const char*>()(s.GetStringArray());
		}
	};
	template<> class hash<SV::BasicStringView<char>*>
	{
	public:
		[[nodiscard]] size_t operator()(const SV::BasicStringView<char>* s) const noexcept
		{
			return std::hash<const char*>()(s->GetStringArray());
		}
	};
	template<> class hash<SV::BasicString<char>*>
	{
	public:
		size_t operator()(const SV::BasicString<char>* s) const
		{
			return std::hash<const char*>()(s->GetStringArray());
		}
	};
	template<> class hash<const SV::BasicStringView<char>*>
	{
	public:
		[[nodiscard]] size_t operator()(const SV::BasicStringView<char>* s) const noexcept
		{
			return std::hash<const char*>()(s->GetStringArray());
		}
	};
	template<> class hash<const SV::BasicString<char>*>
	{
	public:
		size_t operator()(const SV::BasicString<char>* s) const
		{
			return std::hash<const char*>()(s->GetStringArray());
		}
	};

	template<> class hash<SV::BasicStringView<wchar_t>>
	{
	public:
		[[nodiscard]] size_t operator()(const SV::BasicStringView<wchar_t> s) const noexcept
		{
			return std::hash<const wchar_t*>()(s.GetStringArray());
		}
	};
	template<> class hash<SV::BasicString<wchar_t>>
	{
	public:
		size_t operator()(const SV::BasicString<wchar_t>& s) const
		{
			return std::hash<const wchar_t*>()(s.GetStringArray());
		}
	};
	template<> class hash<SV::BasicStringView<wchar_t>*>
	{
	public:
		[[nodiscard]] size_t operator()(const SV::BasicStringView<wchar_t>* s) const noexcept
		{
			return std::hash<const wchar_t*>()(s->GetStringArray());
		}
	};
	template<> class hash<SV::BasicString<wchar_t>*>
	{
	public:
		size_t operator()(const SV::BasicString<wchar_t>* s) const
		{
			return std::hash<const wchar_t*>()(s->GetStringArray());
		}
	};
	template<> class hash<const SV::BasicStringView<wchar_t>*>
	{
	public:
		[[nodiscard]] size_t operator()(const SV::BasicStringView<wchar_t>* s) const noexcept
		{
			return std::hash<const wchar_t*>()(s->GetStringArray());
		}
	};
	template<> class hash<const SV::BasicString<wchar_t>*>
	{
	public:
		size_t operator()(const SV::BasicString<wchar_t>* s) const
		{
			return std::hash<const wchar_t*>()(s->GetStringArray());
		}
	};

	// template<> class hash<SV::BasicStringView<char16_t>>
	// {
	// public:
	// 	[[nodiscard]] size_t operator()(const SV::BasicStringView<char16_t> s) const noexcept
	// 	{
	// 		return _Hash_array_representation(s.GetStringArray(), s.GetLength());
	// 	}
	// };
	// template<> class hash<SV::BasicString<char16_t>>
	// {
	// public:
	// 	size_t operator()(const SV::BasicString<char16_t>& s) const
	// 	{
	// 		return _Hash_array_representation(s.GetStringArray(), s.GetLength());
	// 	}
	// };
	// template<> class hash<SV::BasicStringView<char16_t>*>
	// {
	// public:
	// 	[[nodiscard]] size_t operator()(const SV::BasicStringView<char16_t>* s) const noexcept
	// 	{
	// 		return _Hash_array_representation(s->GetStringArray(), s->GetLength());
	// 	}
	// };
	// template<> class hash<SV::BasicString<char16_t>*>
	// {
	// public:
	// 	size_t operator()(const SV::BasicString<char16_t>* s) const
	// 	{
	// 		return _Hash_array_representation(s->GetStringArray(), s->GetLength());
	// 	}
	// };
	// template<> class hash<const SV::BasicStringView<char16_t>*>
	// {
	// public:
	// 	[[nodiscard]] size_t operator()(const SV::BasicStringView<char16_t>* s) const noexcept
	// 	{
	// 		return _Hash_array_representation(s->GetStringArray(), s->GetLength());
	// 	}
	// };
	// template<> class hash<const SV::BasicString<char16_t>*>
	// {
	// public:
	// 	size_t operator()(const SV::BasicString<char16_t>* s) const
	// 	{
	// 		return _Hash_array_representation(s->GetStringArray(), s->GetLength());
	// 	}
	// };

	// template<> class hash<SV::BasicStringView<char32_t>>
	// {
	// public:
	// 	[[nodiscard]] size_t operator()(const SV::BasicStringView<char32_t> s) const noexcept
	// 	{
	// 		return _Hash_array_representation(s.GetStringArray(), s.GetLength());
	// 	}
	// };
	// template<> class hash<SV::BasicString<char32_t>>
	// {
	// public:
	// 	size_t operator()(const SV::BasicString<char32_t>& s) const
	// 	{
	// 		return _Hash_array_representation(s.GetStringArray(), s.GetLength());
	// 	}
	// };
	// template<> class hash<SV::BasicStringView<char32_t>*>
	// {
	// public:
	// 	[[nodiscard]] size_t operator()(const SV::BasicStringView<char32_t>* s) const noexcept
	// 	{
	// 		return _Hash_array_representation(s->GetStringArray(), s->GetLength());
	// 	}
	// };
	// template<> class hash<SV::BasicString<char32_t>*>
	// {
	// public:
	// 	size_t operator()(const SV::BasicString<char32_t>* s) const
	// 	{
	// 		return _Hash_array_representation(s->GetStringArray(), s->GetLength());
	// 	}
	// };
	// template<> class hash<const SV::BasicStringView<char32_t>*>
	// {
	// public:
	// 	[[nodiscard]] size_t operator()(const SV::BasicStringView<char32_t>* s) const noexcept
	// 	{
	// 		return _Hash_array_representation(s->GetStringArray(), s->GetLength());
	// 	}
	// };
	// template<> class hash<const SV::BasicString<char32_t>*>
	// {
	// public:
	// 	size_t operator()(const SV::BasicString<char32_t>* s) const
	// 	{
	// 		return _Hash_array_representation(s->GetStringArray(), s->GetLength());
	// 	}
	// };
}


#include"String.cpp"