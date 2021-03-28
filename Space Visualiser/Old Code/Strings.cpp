#include"SV/Precompiled Global.h"
#include"Strings.h"
#include"SV/Utilities/Errors.h"
#include"SVUtilities.h"

//
//
#pragma region Global String Operators
//
SV::String operator""_sv(const char* stringA, size_t length)
{
	return std::move(SV::String(stringA));
}
SV::StringView operator""_sb(const char* stringA, std::size_t length)
{
	return SV::StringView(stringA);
}
//
SV::String operator+(const char* str1, const SV::StringView str2)
{
	return std::move(SV::String::Concatenate(str1, str2));
}
SV::String operator+(char* str1, const SV::StringView str2)
{
	return std::move(SV::String::Concatenate(str1, str2));
}
//
bool operator== (const char* str1, const SV::StringView str2)
{
	return SV::String::Equals(str1, str2);
}
bool operator== (char* str1, const SV::StringView str2)
{
	return SV::String::Equals(str1, str2);
}
//
bool operator!= (const char* str1, const SV::StringView str2)
{
	return SV::String::NotEqual(str1, str2);
}
bool operator!= (char* str1, const SV::StringView str2)
{
	return SV::String::NotEqual(str1, str2);
}
//
std::ostream& operator<<(std::ostream& out, const SV::StringView str)
{
	return out << str.GetStringArray();
}
std::istream& operator>>(std::istream& in, SV::String& str)
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
//
#pragma region SV::StringView Class

#pragma region Static Variables
//
const SV::StringView SV::StringView::EmptyString = {};
//
#pragma endregion

#pragma region Constructors and Destructors

SV::StringView::StringView(const char* str)
{
	this->operator=(str);
}

SV::StringView::StringView(const char* str, size_t lengthIn)
{
	SV_ASSERT_T(lengthIn == IndexUndefined, SV::StringView(const char*, size_t), "Length of string to construct SV::StringView is invalid")

		stringData = const_cast<char*>(str);
	length = lengthIn;
}

SV::StringView::StringView(const std::string& str)
{
	this->operator=(str);
}

SV::StringView::StringView(const SV::StringView& obj)
{
	this->operator=(obj);
}

//
SV::StringView::~StringView() {}

#pragma endregion

#pragma region Static String Operations
//
constexpr size_t SV::StringView::GetCStringLength(const char* string)
{
	size_t i = 0;
	for (; string[i] != StringNull || i == IndexUndefined; i++);
	return i;
}
//
bool SV::StringView::Equals(const StringView& str1, const StringView& str2)
{
	if (str1.length == str2.length)
	{
		for (size_t i = 0; i < str1.length; i++)
		{
			if (str1.stringData[i] != str2.stringData[i])
			{
				return false;
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}
//
bool SV::StringView::NotEqual(const StringView& str1, const StringView& str2)
{
	if (str1.length == str2.length)
	{
		for (size_t i = 0; i < str1.length; i++)
		{
			if (str1.stringData[i] != str2.stringData[i])
			{
				return true;
			}
		}
		return false;
	}
	else
	{
		return true;
	}
}
//
SV::Array<size_t> SV::StringView::CalculateFindTable(const StringView& pattern)
{
	SV_ASSERT_T(pattern.GetLength() == 0, SV::StringView::CalculateFindTable(const SV::StringView&), "Pattern length cannot be 0 when creating find table")

		SV::Array<size_t> table(pattern.length, true);
	CalculateFindTable(pattern, table);
	return static_cast<SV::Array<size_t>&&>(table);
}
//
void SV::StringView::CalculateFindTable(const StringView& pattern, SV::Array<size_t>& table)
{
	SV_ASSERT_T(pattern.GetLength() == 0, SV::StringView::CalculateFindTable(const SV::StringView&, SV::Array<size_t>&), "Pattern length cannot be 0 when creating find table")

		if (table.GetCount() == 0) table.Resize(pattern.length);

	table[0] = 0;
	if (table.GetCount() >= 2)
	{
		table[0] = 1;

		for (size_t i = 2; i < pattern.length; i++)
		{
			size_t j = table[i - 1];
			while (true)
			{
				if (pattern.stringData[j] == pattern.stringData[i - 1])
				{
					table[j + 1] = i;
					break;
				}
				if (j == 0)
				{
					table[0] = i;
					break;
				}
				j = table[j];
			}
		}
	}
}
//
size_t SV::StringView::Find(const StringView& searchString, const StringView& pattern, size_t offset)
{
	SV_ASSERT_T(!searchString.IsValid(), SV::StringView::Find, "Search string in find function is nullptr");
	SV_ASSERT_T(!pattern.IsValid(), SV::StringView::Find, "Pattern string in find function is nullptr");
	if (pattern.length > searchString.length || offset > searchString.length - pattern.length)
	{
		return IndexUndefined;
	}

	size_t i = 0;
	SV::Array<size_t> table(pattern.length, true);

	CalculateFindTable(pattern, table);

	while (offset != searchString.length)
	{
		if (searchString.stringData[offset] == pattern.stringData[i])
		{
			i++;
			offset++;
			if (i == pattern.length)
			{
				return offset - pattern.length;
			}

		}
		else if (i > 0)
		{
			i = table[i];
		}
		else
		{
			offset++;
		}
	}

	return IndexUndefined;
}
//
size_t SV::StringView::Find(const StringView& searchString, const StringView& pattern, SV::Array<size_t>& table, size_t offset)
{
	//**
	if (!searchString.IsValid() || !pattern.IsValid() || pattern.length > searchString.length || offset > searchString.length - pattern.length || table.GetCount() != pattern.length)
	{
		return IndexUndefined;
	}

	size_t i = 0;

	try
	{
		while (offset != searchString.length)
		{
			if (searchString.stringData[offset] == pattern.stringData[i])
			{
				i++;
				offset++;
				if (i == pattern.length)
				{
					return offset - pattern.length;
				}
			}
			else if (i > 0)
			{
				i = table[i];
			}
			else
			{
				offset++;
			}
		}
	}
	catch (...)
	{
		return IndexUndefined;
	}

	return IndexUndefined;
}
//
SV::Array<size_t> SV::StringView::FindAll(const StringView& searchString, const StringView& pattern)
{
	if (!searchString.IsValid() || !pattern.IsValid() || pattern.length > searchString.length)
	{
		return SV::Array<size_t>{ IndexUndefined };
	}

	SV::Array<size_t> indicies;
	SV::Array<size_t> table(pattern.length, true);
	CalculateFindTable(pattern, table);

	size_t offset = String::Find(searchString, pattern, table, 0ui32);
	if (offset == IndexUndefined)
	{
		indicies.Add(IndexUndefined);
		return indicies;
	}

	while (offset != IndexUndefined)
	{
		indicies.Add(offset);
		offset++;
		offset = String::Find(searchString, pattern, table, offset);
	}

	return indicies;
}
//
bool SV::StringView::Contains(const StringView& searchString, const StringView& stringToFind)
{
	return Find(searchString, stringToFind) != IndexUndefined;
}
//
SV::StringView SV::StringView::Substring(const StringView& stringIn, size_t index, size_t lengthIn)
{
	if (!stringIn.IsValid() || index > stringIn.length || lengthIn + index > stringIn.length)
	{
		return EmptyString;
	}

	return StringView(stringIn.stringData + index, lengthIn);
}

#pragma endregion

#pragma region Instantiated String Operations
//
bool SV::StringView::StartsWith(char c) const { return stringData[0] == c; }
//
bool SV::StringView::StartsWith(const SV::StringView& str) const
{
	if (length < str.length)
	{
		return false;
	}

	for (size_t i = 0; i < str.length; i++)
	{
		if (stringData[i] != str.stringData[i])
		{
			return false;
		}
	}
	return true;
}
//
bool SV::StringView::EndsWith(char c) const { return stringData[length] == c; }
//
bool SV::StringView::EndsWith(const SV::StringView& str) const
{
	if (length < str.length)
	{
		return false;
	}

	for (size_t i = length; i > 0; i--)
	{
		if (i == length - str.length)
		{
			return true;
		}
		if (stringData[i - 1] != str.stringData[(i - (length - str.length)) - 1])
		{
			return false;
		}
	}
	return true;
}
bool SV::StringView::Equals(const SV::StringView& str1) const { return Equals((*this), str1); }

bool SV::StringView::NotEqual(const SV::StringView& str1) const { return NotEqual((*this), str1); }
//
size_t SV::StringView::Find(const SV::StringView& pattern, size_t offset) const { return Find((*this), pattern, offset); }
//
size_t SV::StringView::Find(const SV::StringView& pattern, SV::Array<size_t>& table, size_t offset) const { return Find((*this), pattern, table, offset); }
//
SV::Array<size_t> SV::StringView::FindAll(const StringView& stringToFind) const { return FindAll((*this), stringToFind); }
//
bool SV::StringView::Contains(const StringView& stringToFind) const { return Find(stringToFind) != IndexUndefined; }
//
SV::StringView SV::StringView::Substring(size_t index, size_t lengthIn) const { return Substring((*this), index, lengthIn); }
//
std::string SV::StringView::ToStdString() const
{
	return std::move(std::string(this->stringData, this->length));
}
//
#pragma endregion

#pragma region Array Access Operators
//
const char& SV::StringView::operator[](size_t index) const
{
	SV_ASSERT_T(index + 1 > length, SV::StringView::operator[] const, "Index out of bounds");
	return stringData[index];
}
//
const char& SV::StringView::Get(size_t index) const
{
	SV_ASSERT_T(index + 1 > length, SV::StringView::Get() const, "Index out of bounds");
	return stringData[index];
}
//
const char& SV::StringView::First() const
{
	return stringData[0];
}
//
const char& SV::StringView::Last() const
{
	return stringData[length];
}
//

#pragma endregion

#pragma region Operator Overloads

//
SV::String SV::StringView::operator+(const StringView& str1) const
{
	return std::move(SV::String::Concatenate((*this), str1));
}
//
SV::String SV::StringView::operator+(const char* str1) const
{
	return std::move(SV::String::Concatenate((*this), str1));
}
//
SV::StringView::operator const char* () const { return (const char*)stringData; }
SV::StringView::operator std::string() const { return std::move(this->ToStdString()); }
//
bool SV::StringView::operator==(const StringView& str1) const
{
	return SV::StringView::Equals((*this), str1);
}
//
bool SV::StringView::operator==(const char* str1) const
{
	return Equals((*this), str1);
}
//
bool SV::StringView::operator!=(const StringView& str1) const
{
	return NotEqual((*this), str1);
}
//
bool SV::StringView::operator!=(const char* str1) const
{
	return NotEqual((*this), StringView(str1));
}
//
void SV::StringView::operator=(const StringView& obj)
{
	stringData = obj.stringData;
	length = obj.length;
}
//
constexpr void SV::StringView::operator=(const char* str)
{
	SV_ASSERT_T(str == nullptr, SV::StringView::operator=(const char*), "Cannot construct a SV::StringView instance with a nullptr pointer");
	stringData = const_cast<char*>(str);
	length = GetCStringLength(str);
	SV_ASSERT_T(length == IndexUndefined, SV::StringView::operator=(const char*), "String pointer has an invalid string length");
}
//
void SV::StringView::operator=(const std::string& str)
{
	stringData = const_cast<char*>(str.c_str());
	length = str.length();
	std::string;
}
//

#pragma endregion

#pragma region Miscellaneous Functions
//
size_t SV::StringView::GetLength() const { return length; }
//
const char* SV::StringView::GetStringArray() const { return stringData; }
//
SV::Array<char> SV::StringView::GetNewStringArray() const { return static_cast<SV::Array<char>&&>(SV::Array<char>(stringData, (size_t)this->length, true)); }
//
char* SV::StringView::begin() { return &stringData[0]; }
const char* SV::StringView::begin() const { return &stringData[0]; }
//
char* SV::StringView::end() { return &stringData[length]; }
const char* SV::StringView::end() const { return &stringData[length]; }
//
bool SV::StringView::IsValid() const { return length != 0; }
//
void SV::StringView::PrintToConsole() const
{
	std::cout << stringData << std::endl;
}

#pragma endregion

#pragma endregion
//
#pragma region String Class

#pragma region Memory Management

void SV::String::Allocate(size_t size)
{
	if (size != IndexUndefined)
	{
		if (size == 0)
		{
			if (stringData != nullptr && stringData != StringView::EmptyString.stringData) free(stringData);
			this->stringData = StringView::EmptyString.stringData;
			this->length = 0;
		}
		else
		{
			if (stringData != nullptr && stringData != StringView::EmptyString.stringData)
			{
				char* temp = (char*)realloc(stringData, size + 1);
				if (temp != nullptr) stringData = temp;
			}
			else
			{
				stringData = (char*)calloc(size + 1, 1);
			}
			if (stringData == nullptr)
			{
				SV_FATAL_T(SV::String::Allocate, "Memory for SV::String could not be allocated.")
					throw std::overflow_error("Memory for SV::String could not be allocated.");
			}

			length = size;
			stringData[length] = StringNull;
		}
	}
	else
	{
		SV_FATAL_T(SV::String::Allocate, "Cannot allocate a string size of 0")
	}
}
//
void SV::String::Delete()
{
	if (stringData != nullptr && shouldDelete && length > 0)
	{
		free(stringData);
		stringData = nullptr;
		length = 0;
	}
}

#pragma endregion

#pragma region Constructors & Destructors
//
SV::String::String() :shouldDelete(true)
{
	this->stringData = StringView::EmptyString.stringData;
	length = 0;
}
//
SV::String::String(char c) : shouldDelete(true)
{
	Allocate(1ui32);
	stringData[0] = c;
}
//
SV::String::String(size_t size) : shouldDelete(true)
{
	Allocate(size);
}
//
SV::String::String(const char* string) : shouldDelete(true)
{
	this->operator=(string);
}
//
SV::String::String(const std::string& string) : shouldDelete(true)
{
	this->operator=(string);
}
//
SV::String::String(const SV::StringView& string) : shouldDelete(true)
{
	this->operator=(string);
}
//
SV::String::String(const SV::Array<char>& string) : shouldDelete(true)
{
	Allocate(string.GetCount());
	for (size_t i = 0; i < length; i++)
	{
		stringData[i] = string[i];
	}
}
//
SV::String::String(const String& string) : shouldDelete(true)
{
	this->operator=(string);
}
//
SV::String::String(String&& string) noexcept : shouldDelete(true)
{
	this->operator=(static_cast<SV::String&&>(string));
}
//Private constructor
SV::String::String(const char* stringPtr, size_t size, bool shouldDelete)
{
	if (stringPtr == nullptr)
	{
		SV_FATAL_T(SV::String::String(const char*, size_t, bool), "Input char array cannot be nullptr.");
	}

	length = size;
	if (length == IndexUndefined)
	{
		SV_FATAL_T(SV::String::String(const char*, size_t, bool), "Constructor string length invalid");
	}
	stringData = const_cast<char*>(stringPtr);
	this->shouldDelete = shouldDelete;
}
//Private constructor
SV::String::String(char* stringPtr, size_t size, bool shouldDelete)
{
	if (stringPtr == nullptr)
	{
		SV_FATAL_T(SV::String::String(char*, size_t, bool), "Input char array cannot be nullptr.");
	}

	length = size;
	if (length == IndexUndefined)
	{
		SV_FATAL_T(SV::String::String(char*, size_t, bool), "Constructor string length invalid");
	}
	stringData = stringPtr;
	stringData[length] = StringNull;
	this->shouldDelete = shouldDelete;
}
//
SV::String::~String()
{
	this->Delete();
}

#pragma endregion

#pragma region Static String Operations
//
SV::String SV::String::Concatenate(const StringView& str1, const StringView& str2)
{
	String returnString(str1.length + str2.length);
	//
	size_t i = 0;
	//
	for (i = 0; i < str1.length; i++)
	{
		returnString.stringData[i] = str1.stringData[i];
	}
	//
	for (; i < returnString.length; i++)
	{
		returnString.stringData[i] = str2.stringData[i - str1.length];
	}
	//
	return std::move(returnString);
}
//
void SV::String::Append(String& str1, const StringView& str2)
{
	size_t previousLength = str1.length;
	str1.Allocate(previousLength + str2.length);
	//
	for (size_t i = previousLength; i < str1.length; i++)
	{
		str1.stringData[i] = str2.stringData[i - previousLength];
	}
}
//
void SV::String::Append(String& str1, const char c)
{
	size_t previousLength = str1.length;
	str1.Allocate(str1.length + 1);

	str1.stringData[previousLength] = c;
	str1.stringData[str1.length] = 0;
}
//
SV::Array<SV::String> SV::String::Split(const StringView& stringIn, const StringView& delimiter)
{
	if (delimiter.length > stringIn.length || delimiter.length <= 0)
	{
		return SV::Array<String>{ EmptyString };
	}

	SV::Array<String> strings;
	SV::Array<size_t> table(delimiter.length);
	CalculateFindTable(delimiter, table);

	size_t j = String::Find(stringIn, delimiter, table, 0ui32);
	size_t i = 0;

	if (j == IndexUndefined)
	{
		strings.Add(stringIn);
		return strings;
	}

	while (j != IndexUndefined)
	{
		if (i < j)
		{
			strings.Emplace(String::Substring(stringIn, i, j - i));
		}
		j += delimiter.length;
		i = j;
		j = String::Find(stringIn, delimiter, table, j);
	}

	if (i != IndexUndefined && i < stringIn.length)
	{
		strings.Emplace(String::Substring(stringIn, i, stringIn.length - i));
	}

	return std::move(strings);
}
//
size_t SV::String::GetWordCount(const StringView& stringIn)
{
	if (!stringIn.IsValid()) return IndexUndefined;
	//
	SV::Array<size_t> indicies = FindAll(stringIn, StringView(" "));
	if (indicies[0] == IndexUndefined) return 1;
	//
	size_t currentCount = 0;
	//
	if (!iscntrl(stringIn.stringData[0]) && stringIn.stringData[0] != ' ') currentCount++;
	//
	for (size_t& ii : indicies)
	{
		if (!iscntrl(stringIn.stringData[ii + 1]) && stringIn.stringData[ii + 1] != ' ')
		{
			currentCount++;
		}
	}
	//
	return currentCount;
}
///
SV::Array<SV::String> SV::String::GetWords(const StringView& stringIn)
{
	return static_cast<SV::Array<String>&&>(Split(stringIn, String(" ", 1, false)));
}
//
SV::String SV::String::Replace(const StringView& string, const StringView& oldString, const StringView& newString)
{
	String returnString = string;
	ReplaceAssign(returnString, oldString, newString);
	return std::move(returnString);
}
//
void SV::String::ReplaceAssign(String& stringIn, const StringView& oldString, const StringView& newString)
{
	if (stringIn.IsValid() && oldString.IsValid())
	{
		SV::Array<size_t> indicies = FindAll(stringIn, oldString);
		if (indicies[0] != IndexUndefined)
		{
			size_t j = 0;
			size_t k = 0;
			size_t o = 0;
			SV::Array<char> buffer(stringIn.length);
			for (; j < stringIn.length; j++)
			{
				buffer[j] = stringIn.stringData[j];
			}
			j = 0;

			stringIn.length += int((indicies.GetCount() * newString.length) - (indicies.GetCount() * oldString.length));
			char* newStringData = (char*)realloc(stringIn.stringData, stringIn.length + 1);
			if (newStringData == nullptr)
			{
				SV_FATAL_T(SV::String::ReplaceAssign(String&, const SV::StringView&, const SV::StringView&), "Memory could not be allocated for string.");
				throw std::overflow_error("Memory could not be allocated for string.");
			}
			stringIn.stringData = newStringData;

			for (size_t i = 0; i < stringIn.length; i++)
			{
				if (k >= indicies.GetCount())
				{
					goto CopyFromBuffer;
				}
				else if (j == indicies[k])
				{
					for (o = 0; o < newString.length; o++)
					{
						stringIn.stringData[i + o] = newString.stringData[o];
					}
					i += o - 1;
					j = indicies[k] + oldString.length;
					k++;
				}
				else
				{
				CopyFromBuffer:
					stringIn.stringData[i] = buffer[j];
					j++;
				}
			}

			stringIn.stringData[stringIn.length] = StringNull;
		}
	}
}
//
SV::String SV::String::Remove(const StringView& string, const StringView& oldString)
{
	String returnString = string;
	ReplaceAssign(returnString, oldString, EmptyString);
	return std::move(returnString);
}
//
void SV::String::RemoveAssign(String& string, const StringView& oldString)
{
	ReplaceAssign(string, oldString, EmptyString);
}
//
SV::String SV::String::Join(const SV::Array<StringView>& strings)
{
	return std::move(Join(EmptyString, strings));
}
//
SV::String SV::String::Join(const StringView& joinDelimiter, const SV::Array<StringView>& strings)
{
	size_t i = 0;
	size_t newSize = 0;
	for (i = 0; i < strings.GetCount(); i++)
	{
		newSize += strings[i].length;
	}

	newSize += (joinDelimiter.length * (strings.GetCount() - 1));

	String returnString(newSize);

	size_t currentLoc = 0;

	for (i = 0; i < strings.GetCount() - 1; i++)
	{
		memcpy(returnString.stringData + currentLoc, strings[i].stringData, strings[i].length);
		currentLoc += strings[i].length;
		memcpy(returnString.stringData + currentLoc, joinDelimiter.stringData, joinDelimiter.length);
		currentLoc += joinDelimiter.length;
	}

	memcpy(returnString.stringData + currentLoc, strings.Last().stringData, strings.Last().length);

	return std::move(returnString);
}
//
SV::String SV::String::Insert(const StringView& string, const StringView& stringToInsert, size_t index)
{
	if (index < string.length + 1)
	{
		String returnString(string.length + stringToInsert.length);
		memcpy(returnString.stringData, string.stringData, index);
		memcpy(returnString.stringData + index, stringToInsert.stringData, stringToInsert.length);
		memcpy(returnString.stringData + index + stringToInsert.length, string.stringData + index, string.length - index);
		return std::move(returnString);
	}
	else
	{
		return EmptyString;
	}
}
//
void SV::String::InsertAssign(String& string, const StringView& stringToInsert, size_t index)
{
	if (index < string.length + 1)
	{
		size_t originalLength = string.length;
		string.Allocate(string.length + stringToInsert.length);
		char* buffer = (char*)malloc(originalLength - index);

		memcpy(buffer, string.stringData + index, originalLength - index);
		memcpy(string.stringData + index, stringToInsert.stringData, stringToInsert.length);
		memcpy(string.stringData + index + stringToInsert.length, buffer, originalLength - index);
	}
}
//
SV::String SV::String::Splice(const StringView& string, size_t index, size_t length)
{
	String returnString = string;
	SpliceAssign(returnString, index, length);
	return std::move(returnString);
}
//
void SV::String::SpliceAssign(String& string, size_t index, size_t length)
{
	if (length + index <= string.length)
	{
		string.operator=(String::Substring(string, 0, index) + String::Substring(string, index + length, string.length - (index + length)));
	}
}
//
SV::String SV::String::Reverse(const StringView& string)
{
	String newString;
	newString.Allocate(string.length);
	//
	size_t i = string.length;
	size_t j = 0;
	//
	for (; i > 0; i--)
	{
		newString.stringData[j] = string.stringData[i - 1];
		j++;
	}
	//
	return std::move(newString);
}
//
void SV::String::ReverseAssign(String& string)
{
	SV::Array<char> newString(string.length);
	//
	size_t i = string.length;
	size_t j = 0;
	//
	for (; i > 0; i--)
	{
		newString[j] = string.stringData[i - 1];
		j++;
	}
	//
	for (i = 0; i < string.length; i++)
	{
		string.stringData[i] = newString[i];
	}
}
//
SV::String SV::String::ToUpper(const StringView& str)
{
	String returnString = str;
	ToUpperAssign(returnString);
	return std::move(returnString);
}
//
void SV::String::ToUpperAssign(String& string)
{
	for (size_t i = 0; i < string.length; i++)
	{
		string.stringData[i] = CharToUpper(string.stringData[i]);
	}
}
//
SV::String SV::String::ToLower(const StringView& str)
{
	String returnString = str;
	ToLowerAssign(returnString);
	return std::move(returnString);
}
//
void SV::String::ToLowerAssign(String& string)
{
	for (size_t i = 0; i < string.length; i++)
	{
		string.stringData[i] = CharToLower(string.stringData[i]);
	}
}

#pragma endregion

#pragma region Instantiated String Operations
//
SV::String SV::String::Concatenate(const StringView& str1) const { return Concatenate((*this), str1); }
//
void SV::String::Append(const StringView& str1) { Append(*this, str1); }
void SV::String::Append(const char c) { Append(*this, c); }
//
SV::Array<SV::String> SV::String::Split(const StringView& delimiter) const { return std::move(Split((*this), delimiter)); }
//
size_t SV::String::GetWordCount() const { return GetWordCount((*this)); }
//
SV::Array<SV::String> SV::String::GetWords() const { return std::move(GetWords((*this))); }
//
SV::String SV::String::Replace(const StringView& oldString, const StringView& newString) const { return Replace((*this), oldString, newString); }
//
void SV::String::ReplaceAssign(const StringView& oldString, const StringView& newString) { ReplaceAssign(*this, oldString, newString); }
//
SV::String SV::String::Remove(const StringView& oldString) const { return Remove((*this), oldString); }
//
void SV::String::RemoveAssign(const StringView& oldString) { RemoveAssign(*this, oldString); }
//
SV::String SV::String::Insert(const StringView& stringToInsert, size_t index) const { return Insert((*this), stringToInsert, index); }
//
void SV::String::InsertAssign(const StringView& stringToInsert, size_t index) { InsertAssign(*this, stringToInsert, index); }
//
SV::String SV::String::Splice(size_t index, size_t length) const { return Splice((*this), index, length); }
//
void SV::String::SpliceAssign(size_t index, size_t length) { SpliceAssign(*this, index, length); }
//
SV::String SV::String::Reverse() const { return Reverse((*this)); }
//
void SV::String::ReverseAssign() { ReverseAssign(*this); }
//
SV::String SV::String::ToUpper() const { return ToUpper((*this)); }
//
void SV::String::ToUpperAssign() { ToUpperAssign(*this); }
//
SV::String SV::String::ToLower() const { return ToLower((*this)); }
//
void SV::String::ToLowerAssign() { ToLowerAssign(*this); }

#pragma endregion

#pragma region Operator Overloads

void SV::String::operator=(const SV::String& string)
{
	Allocate(string.length);

	memcpy(this->stringData, string.stringData, string.length * sizeof(char));
}
void SV::String::operator=(const StringView string)
{
	Allocate(string.length);

	memcpy(this->stringData, string.stringData, string.length * sizeof(char));
}
void SV::String::operator=(const std::string& string)
{
	Allocate(string.length());

	memcpy(this->stringData, string.c_str(), string.length() * sizeof(char));
}
void SV::String::operator=(const std::stringstream& ss) noexcept
{
	this->operator=(std::move(ss.str()));
}
void SV::String::operator+=(const StringView str1)
{
	Append(*this, str1);
}
//
void SV::String::operator=(SV::String&& string) noexcept
{
	if (this != &string)
	{
		this->Delete();

		this->stringData = string.stringData;
		this->length = string.length;

		string.stringData = nullptr;
		string.length = 0;
	}
}
void SV::String::operator=(std::string&& stringIn) noexcept
{
	this->Delete();
	this->stringData = (char*)stringIn.c_str();
	this->length = stringIn.size();

	memset(&stringIn, 0, sizeof(std::string));
}
//
void SV::String::operator=(const char* string)
{
	size_t sLength = GetCStringLength(string);

	Allocate(sLength);

	memcpy(this->stringData, string, sLength * sizeof(char));
}
//
void SV::String::operator+=(const char* str1)
{
	Append(*this, String(str1, GetCStringLength(str1), false));
}
//
SV::String::operator char* () { return stringData; }

#pragma endregion

#pragma region Miscellaneous Functions

void SV::String::Clear()
{
	Allocate(0ui32);
}

#pragma endregion

#pragma region Mutable Array Access Functions
//
char& SV::String::operator[](size_t index)
{
	SV_ASSERT_T(index + 1 > length, SV::String::operator[], "Index out of bounds");
	return stringData[index];
}
//
char& SV::String::Get(size_t index)
{
	SV_ASSERT_T(index + 1 > length, SV::String::Get, "Index out of bounds");
	return stringData[index];
}
//
void SV::String::Set(size_t index, char c)
{
	SV_ASSERT_T(index + 1 > length, SV::String::Set, "Index out of bounds");
	stringData[index] = c;
}
//
char& SV::String::First()
{
	return stringData[0];
}
//
char& SV::String::Last()
{
	return stringData[length];
}
//
#pragma endregion

#pragma endregion
//