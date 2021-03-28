#include"SV/Precompiled Global.hpp"
#include "String.hpp"

#include<string>
#include<iostream>

//Explicit Instantiations
template class SV::BasicStringView<char>;
template class SV::BasicStringView<wchar_t>;
#ifdef __cpp_char8_t
template class SV::BasicStringView<char8_t>;
#endif // __cpp_char8_t 
template class SV::BasicStringView<char16_t>;
template class SV::BasicStringView<char32_t>;

template class SV::BasicString<char>;
template class SV::BasicString<wchar_t>;
#ifdef __cpp_char8_t
template class SV::BasicString<char8_t>;
#endif // __cpp_char8_t 
template class SV::BasicString<char16_t>;
template class SV::BasicString<char32_t>;

//
//
namespace SV
{
	//
#pragma region SV::BasicStringView Class

#pragma region Static Variables
//
	template<typename T>
	const BasicStringView<T> BasicStringView<T>::EmptyString;
	//
#pragma endregion

#pragma region Constructors and Destructors

	template<typename T>
	BasicStringView<T>::BasicStringView(const T* str)
	{
		this->operator=(str);
	}
	template<typename T>
	BasicStringView<T>::BasicStringView(const T* str, size_t lengthIn)
	{
		SV_ASSERT_T(lengthIn == IndexUndefined, SV::StringView(const char*, size_t), "Length of string to construct SV::StringView is invalid")

			this->stringData = const_cast<T*>(str);
		this->stringLength = lengthIn;
	}
	template<typename T>
	BasicStringView<T>::BasicStringView(const std::initializer_list<T>& list)
	{
		this->stringData = const_cast<T*>(list.begin());
		this->stringLength = list.size();
	}
	template<typename T>
	BasicStringView<T>::BasicStringView(const std::basic_string_view<T> stringView)
	{
		this->stringData = const_cast<T*>(stringView.data());
		this->stringLength = stringView.size();
	}
	template<typename T>
	BasicStringView<T>::BasicStringView(const std::basic_string<T>& str)
	{
		this->operator=(str);
	}
	template<typename T>
	BasicStringView<T>::BasicStringView(const BasicStringView<T>& obj)
	{
		this->stringData = obj.stringData;
		this->stringLength = obj.stringLength;
	}
	template<typename T>
	BasicStringView<T>::BasicStringView(const BasicString<T>& obj)
	{
		this->stringData = obj.stringData;
		this->stringLength = obj.stringLength;
	}

#pragma endregion

#pragma region Static String Operations
	//
	template<typename T>
	constexpr size_t BasicStringView<T>::GetCStringLength(const T* string)
	{
		static_assert(std::is_integral_v<T>, "This function cannot be employed on non-intergral types");

		size_t i = 0;
		for (; string[i] != StringNull || i == IndexUndefined; i++);
		return i;
	}
	//
	template<typename T>
	bool BasicStringView<T>::Equals(const BasicStringView<T>& str1, const BasicStringView<T>& str2)
	{
		if (str1.stringLength == str2.stringLength)
		{
			for (size_t i = 0; i < str1.stringLength; i++)
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
	template<typename T>
	bool BasicStringView<T>::NotEqual(const BasicStringView<T>& str1, const BasicStringView<T>& str2)
	{
		if (str1.stringLength == str2.stringLength)
		{
			for (size_t i = 0; i < str1.stringLength; i++)
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
	template<typename T>
	SV::Array<size_t> BasicStringView<T>::CalculateFindTable(const BasicStringView<T>& pattern)
	{
		SV_ASSERT_T(pattern.GetLength() == 0, BasicStringView<T>::CalculateFindTable(const BasicStringView<T>&), "Pattern length cannot be 0 when creating find table");

		SV::Array<size_t> table(pattern.stringLength, true);
		CalculateFindTable(pattern, table);
		return std::move(table);
	}
	//
	template<typename T>
	void BasicStringView<T>::CalculateFindTable(const BasicStringView<T>& pattern, SV::Array<size_t>& table)
	{
		SV_ASSERT_T(pattern.GetLength() == 0, BasicStringView<T>::CalculateFindTable(const BasicStringView<T>&, SV::Array<size_t>&), "Pattern length cannot be 0 when creating find table")

			if (table.GetCount() == 0) table.Resize(pattern.stringLength);

		table[0] = 0;
		if (table.GetCount() >= 2)
		{
			table[0] = 1;

			for (size_t i = 2; i < pattern.stringLength; i++)
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
	template<typename T>
	size_t BasicStringView<T>::Find(const BasicStringView<T>& searchString, const BasicStringView<T>& pattern, size_t offset)
	{
		SV_ASSERT_T(!searchString.IsEmpty(), BasicStringView<T>::Find, "Search string in find function is nullptr");
		SV_ASSERT_T(!pattern.IsEmpty(), BasicStringView<T>::Find, "Pattern string in find function is nullptr");
		if (pattern.stringLength > searchString.stringLength || offset > searchString.stringLength - pattern.stringLength)
		{
			return IndexUndefined;
		}

		size_t i = 0;
		SV::Array<size_t> table(pattern.stringLength, true);

		BasicStringView<T>::CalculateFindTable(pattern, table);

		while (offset != searchString.stringLength)
		{
			if (searchString.stringData[offset] == pattern.stringData[i])
			{
				i++;
				offset++;
				if (i == pattern.stringLength)
				{
					return offset - pattern.stringLength;
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
	template<typename T>
	size_t BasicStringView<T>::Find(const BasicStringView<T>& searchString, const BasicStringView<T>& pattern, SV::Array<size_t>& table, size_t offset)
	{
		//**
		if (!searchString.IsEmpty() || !pattern.IsEmpty() || pattern.stringLength > searchString.stringLength || offset > searchString.stringLength - pattern.stringLength || table.GetCount() != pattern.stringLength)
		{
			return IndexUndefined;
		}

		size_t i = 0;

		try
		{
			while (offset != searchString.stringLength)
			{
				if (searchString.stringData[offset] == pattern.stringData[i])
				{
					i++;
					offset++;
					if (i == pattern.stringLength)
					{
						return offset - pattern.stringLength;
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
	template<typename T>
	SV::Array<size_t> BasicStringView<T>::FindAll(const BasicStringView<T>& searchString, const BasicStringView<T>& pattern)
	{
		if (!searchString.IsEmpty() || !pattern.IsEmpty() || pattern.stringLength > searchString.stringLength)
		{
			return SV::Array<size_t>{ IndexUndefined };
		}

		SV::Array<size_t> indicies;
		SV::Array<size_t> table(pattern.stringLength, true);
		CalculateFindTable(pattern, table);

		size_t offset = BasicStringView<T>::Find(searchString, pattern, table, 0ui32);
		if (offset == IndexUndefined)
		{
			indicies.Add(IndexUndefined);
			return indicies;
		}

		while (offset != IndexUndefined)
		{
			indicies.Add(offset);
			offset++;
			offset = BasicStringView<T>::Find(searchString, pattern, table, offset);
		}

		return std::move(indicies);
	}
	//
	template<typename T>
	bool BasicStringView<T>::Contains(const BasicStringView<T>& searchString, const BasicStringView<T>& stringToFind)
	{
		return Find(searchString, stringToFind) != IndexUndefined;
	}
	//
	template<typename T>
	SV::BasicStringView<T> BasicStringView<T>::Substring(const BasicStringView<T>& stringIn, size_t index, size_t lengthIn)
	{
		if (!stringIn.IsEmpty() || index > stringIn.stringLength || lengthIn + index > stringIn.stringLength)
		{
			return BasicStringView<T>::EmptyString;
		}

		return BasicStringView<T>(stringIn.stringData + index, lengthIn);
	}
	//
	template<typename T>
	SV::Array<BasicStringView<T>> BasicStringView<T>::SplitView(const BasicStringView<T>& stringIn, const BasicStringView<T>& delimiter)
	{
		if (delimiter.stringLength > stringIn.stringLength || delimiter.stringLength <= 0)
		{
			return {};
		}

		SV::Array<BasicStringView<T>> strings;
		SV::Array<size_t> table(delimiter.stringLength);
		BasicStringView<T>::CalculateFindTable(delimiter, table);

		size_t j = BasicStringView<T>::Find(stringIn, delimiter, table, 0ui32);
		size_t i = 0;

		if (j == IndexUndefined)
		{
			strings.Add(stringIn);
			return std::move(strings);
		}

		while (j != IndexUndefined)
		{
			if (i < j)
			{
				strings.Add(BasicStringView<T>::Substring(stringIn, i, j - i));
			}
			j += delimiter.stringLength;
			i = j;
			j = BasicStringView<T>::Find(stringIn, delimiter, table, j);
		}

		if (i != IndexUndefined && i < stringIn.stringLength)
		{
			strings.Add(BasicStringView<T>::Substring(stringIn, i, stringIn.stringLength - i));
		}

		return std::move(strings);
	}
	//
	template<typename T>
	BasicStringView<T> BasicStringView<T>::TrimWhitespaces(const BasicStringView<T>& stringIn)
	{
		size_t beginIndex = 0;
		while (beginIndex + 1 < stringIn.stringLength && stringIn.stringData[beginIndex] == (const T)' ') beginIndex++;

		size_t endIndex = stringIn.stringLength - 1;
		while (endIndex > 0 && stringIn.stringData[endIndex] == (const T)' ') endIndex--;

		return SV::BasicStringView<T>(stringIn.stringData + beginIndex, (endIndex + 1) - beginIndex);
	}
	//

#pragma endregion

#pragma region Instantiated String Operations
	//
	template<typename T>
	bool BasicStringView<T>::StartsWith(T c) const { return this->stringData[0] == c; }
	//
	template<typename T>
	bool BasicStringView<T>::StartsWith(const BasicStringView<T>& str) const
	{
		if (stringLength < str.stringLength)
		{
			return false;
		}

		for (size_t i = 0; i < str.stringLength; i++)
		{
			if (this->stringData[i] != str.stringData[i])
			{
				return false;
			}
		}
		return true;
	}
	//
	template<typename T>
	bool BasicStringView<T>::EndsWith(T c) const { return this->stringData[stringLength] == c; }
	//
	template<typename T>
	bool BasicStringView<T>::EndsWith(const BasicStringView<T>& str) const
	{
		if (stringLength < str.stringLength)
		{
			return false;
		}

		for (size_t i = stringLength; i > 0; i--)
		{
			if (i == stringLength - str.stringLength)
			{
				return true;
			}
			if (this->stringData[i - 1] != str.stringData[(i - (stringLength - str.stringLength)) - 1])
			{
				return false;
			}
		}
		return true;
	}
	template<typename T>
	bool BasicStringView<T>::Equals(const BasicStringView<T>& str1) const { return Equals((*this), str1); }
	template<typename T>
	bool BasicStringView<T>::NotEqual(const BasicStringView<T>& str1) const { return NotEqual((*this), str1); }
	//
	template<typename T>
	size_t BasicStringView<T>::Find(const BasicStringView<T>& pattern, size_t offset) const { return Find((*this), pattern, offset); }
	//
	template<typename T>
	size_t BasicStringView<T>::Find(const BasicStringView<T>& pattern, SV::Array<size_t>& table, size_t offset) const { return Find((*this), pattern, table, offset); }
	//
	template<typename T>
	SV::Array<size_t> BasicStringView<T>::FindAll(const BasicStringView<T>& stringToFind) const { return std::move(FindAll((*this), stringToFind)); }
	//
	template<typename T>
	bool BasicStringView<T>::Contains(const BasicStringView<T>& stringToFind) const { return Find(stringToFind) != IndexUndefined; }
	//
	template<typename T>
	SV::BasicStringView<T> BasicStringView<T>::Substring(size_t index, size_t lengthIn) const { return Substring((*this), index, lengthIn); }
	//
	template<typename T>
	SV::Array<BasicStringView<T>> BasicStringView<T>::SplitView(const BasicStringView<T>& delimeter) const { return std::move(SplitView((*this), delimeter)); }
	//
	template<typename T>
	SV::BasicStringView<T> BasicStringView<T>::TrimWhitespaces() const { return TrimWhitespaces(*this); }
	//
	template<typename T>
	std::basic_string<T> BasicStringView<T>::ToStdString() const
	{
		return std::move(std::basic_string<T>(this->stringData, this->stringLength));
	}
	//
#pragma endregion

#pragma region Array Access Operators
//
	template<typename T>
	const T& BasicStringView<T>::operator[](size_t index) const
	{
		SV_ASSERT_T(index + 1 > stringLength, BasicStringView<T>::operator[] const, "Index out of bounds");
		return this->stringData[index];
	}
	//
	template<typename T>
	const T& BasicStringView<T>::Get(size_t index) const
	{
		SV_ASSERT_T(index + 1 > stringLength, BasicStringView<T>::Get() const, "Index out of bounds");
		return this->stringData[index];
	}
	//
	template<typename T>
	const T& BasicStringView<T>::First() const
	{
		return this->stringData[0];
	}
	//
	template<typename T>
	const T& BasicStringView<T>::Last() const
	{
		return this->stringData[stringLength];
	}
	//

#pragma endregion

#pragma region Operator Overloads
	//
	template<typename T>
	BasicStringView<T>::operator const T* () const { return (const T*)this->stringData; }
	//
	template<typename T>
	bool BasicStringView<T>::operator==(const BasicStringView<T>& str1) const
	{
		return BasicStringView<T>::Equals((*this), str1);
	}
	template<typename T>
	bool BasicStringView<T>::operator==(const T* str1) const
	{
		return Equals((*this), str1);
	}
	//
	template<typename T>
	bool BasicStringView<T>::operator!=(const BasicStringView<T>& str1) const
	{
		return NotEqual((*this), str1);
	}
	template<typename T>
	bool BasicStringView<T>::operator!=(const T* str1) const
	{
		return NotEqual((*this), str1);
	}
	//
	template<typename T>
	void BasicStringView<T>::operator=(const BasicStringView<T>& obj)
	{
		this->stringData = obj.stringData;
		this->stringLength = obj.stringLength;
	}
	template<typename T>
	constexpr void BasicStringView<T>::operator=(const T* str)
	{
		SV_ASSERT_T(str == nullptr, BasicStringView<T>::operator=(const char*), "Cannot construct a SV::StringView instance with a nullptr pointer");
		this->stringData = const_cast<T*>(str);
		stringLength = GetCStringLength(str);
		SV_ASSERT_T(stringLength == IndexUndefined, BasicStringView<T>::operator=(const char*), "String pointer has an invalid string length");
	}
	template<typename T>
	void BasicStringView<T>::operator=(const std::basic_string<T>& str)
	{
		this->stringData = const_cast<T*>(str.c_str());
		stringLength = str.length();
	}
	//

#pragma endregion

#pragma region Miscellaneous Functions
//
	template<typename T>
	size_t BasicStringView<T>::GetLength() const { return stringLength; }
	//
	template<typename T>
	const T* BasicStringView<T>::GetStringArray() const { return this->stringData; }
	//
	template<typename T>
	SV::Array<T> BasicStringView<T>::GetNewStringArray() const { return std::move(SV::Array<T>(this->stringData, (size_t)this->stringLength, true)); }
	//
	template<typename T>
	const T* BasicStringView<T>::begin() const { return &this->stringData[0]; }
	template<typename T>
	const T* BasicStringView<T>::end() const { return &this->stringData[stringLength]; }
	//
	template<typename T>
	bool BasicStringView<T>::IsEmpty() const { return stringLength != 0; }
	//
	template<typename T>
	void BasicStringView<T>::PrintToConsole() const
	{
		std::cout << this->stringData << '\n';
	}
	//
	template<typename T>
	size_t BasicStringView<T>::Hash() const
	{
		return std::_Hash_array_representation<T>(this->stringData, this->stringLength);
	}

#pragma endregion

#pragma endregion
	//
#pragma region SV::BasicString<T> Class

#pragma region Core Utility Functions

	template<typename T>
	void BasicString<T>::Allocate(std::size_t elements)
	{
		if (elements > this->stringLength && this->stringCapacity > elements)
		{
			this->stringLength = elements;
			this->stringData[elements] = 0;
			return;
		}

		if (elements + 1 < STR_OPTIMISE_STORE)
		{
			if (this->allocatorStore == AllocatorStore::Heap && this->stringData != this->charStore)
			{
				memcpy(&optimiseStore[0], this->stringData, (STR_OPTIMISE_STORE - 1) * sizeof(T));
				free((void*)this->stringData);
			}

			this->stringData = (T*)&optimiseStore[0];
			this->allocatorStore = AllocatorStore::SSOptimise;
			this->stringCapacity = STR_OPTIMISE_STORE - 1;
		}
		else
		{
			size_t geometricCapacity = 0;
			if (this->allocatorStore == AllocatorStore::SSOptimise || this->stringData == this->charStore)
			{
				geometricCapacity = elements > 16 ? elements : 16;

				this->stringData = (T*)malloc((geometricCapacity + 1) * sizeof(T));
				if (this->stringData == nullptr) throw std::runtime_error("[FATAL ERROR] Cannot allocate heap memory for string.");

				this->stringCapacity = geometricCapacity;
			}
			else
			{
				geometricCapacity = this->stringCapacity + this->stringCapacity / 2;
				geometricCapacity = geometricCapacity < elements ? elements : geometricCapacity;

				T* tempPtr = (T*)realloc(this->stringData, (geometricCapacity + 1) * sizeof(T));
				if (tempPtr == nullptr) throw std::runtime_error("[FATAL ERROR] Cannot allocate heap memory for string.");

				this->stringData = tempPtr;
				this->stringCapacity = geometricCapacity;
			}

			if (this->allocatorStore == AllocatorStore::SSOptimise)
			{
				memcpy(this->stringData, &this->optimiseStore[0], this->stringLength * sizeof(T));
			}

			this->allocatorStore = AllocatorStore::Heap;
		}

		this->stringLength = elements;
		this->stringData[elements] = 0;
	}

#pragma endregion

#pragma region Constructors & Destructors
	//
	template<typename T>
	BasicString<T>::BasicString()
	{
		Allocate(0);
	}
	//
	template<typename T>
	BasicString<T>::BasicString(T c)
	{
		Allocate(1);
		this->stringData[0] = c;
	}
	//
	template<typename T>
	BasicString<T>::BasicString(size_t size, T fillerChar)
	{
		Allocate(size);
		for (size_t i = 0; i < size; i++)
		{
			this->stringData[i] = fillerChar;
		}
	}
	//
	template<typename T>
	BasicString<T>::BasicString(const T* string)
	{
		Allocate(MyStringView::GetCStringLength(string));
		memcpy(this->stringData, string, (this->stringLength) * sizeof(T));
	}
	//
	template<typename T>
	BasicString<T>::BasicString(const std::string& string)
	{
		Allocate(string.size());
		memcpy(this->stringData, string.c_str(), string.size() * sizeof(T));
	}
	//
	template<typename T>
	BasicString<T>::BasicString(std::basic_string_view<T> stringView)
	{
		Allocate(stringView.size());
		memcpy(this->stringData, stringView.data(), stringView.size() * sizeof(T));
	}
	//
	template<typename T>
	BasicString<T>::BasicString(const std::initializer_list<T>& listIn)
	{
		Allocate(listIn.size());
		memcpy(this->stringData, listIn.begin(), listIn.size() * sizeof(T));
	}
	//
	template<typename T>
	BasicString<T>::BasicString(const BasicStringView<T>& string)
	{
		Allocate(string.stringLength);
		memcpy(this->stringData, string.stringData, string.stringLength * sizeof(T));
	}
	//
	template<typename T>
	BasicString<T>::BasicString(const SV::Array<T>& string)
	{
		Allocate(string.GetCount());
		memcpy(this->stringData, string.GetInternalArray(), string.GetCount() * sizeof(T));
	}
	//
	template<typename T>
	BasicString<T>::BasicString(const BasicString<T>& string)
	{
		Allocate(string.stringLength);
		memcpy(this->stringData, string.stringData, string.stringLength * sizeof(T));
	}
	//
	template<typename T>
	BasicString<T>::BasicString(BasicString<T>&& string) noexcept
	{
		if (string.allocatorStore == AllocatorStore::SSOptimise)
		{
			if (this->allocatorStore == AllocatorStore::Heap && this->stringData != this->charStore) free((void*)this->stringData);
			this->stringData = &this->optimiseStore[0];
			this->allocatorStore = AllocatorStore::SSOptimise;
			this->stringLength = string.stringLength;
			this->stringCapacity = STR_OPTIMISE_STORE - 1;

			memcpy(this->stringData, string.stringData, (string.stringLength + 1) * sizeof(T));
		}
		else
		{
			this->stringData = string.stringData;
			this->stringLength = string.stringLength;
			this->allocatorStore = string.allocatorStore;
			this->stringCapacity = string.stringCapacity;
		}

		string.stringData = nullptr;
		string.stringLength = 0;
		string.stringCapacity = 0;
	}
	//
	template<typename T>
	BasicString<T>::BasicString(const T* stringPtr, size_t size)
	{
		Allocate(size);
		memcpy(this->stringData, stringPtr, size * sizeof(T));
	}
	//
	template<typename T>
	BasicString<T>::~BasicString() noexcept
	{
		if (this->allocatorStore == AllocatorStore::Heap && this->stringData != this->charStore)
		{
			free((void*)this->stringData);
		}
		this->stringData = nullptr;
		this->stringLength = 0;
	}

#pragma endregion

#pragma region Static String Operations
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Concatenate(const BasicStringView<T>& str1, const BasicStringView<T>& str2)
	{
		SV::BasicString<T> returnString(str1.stringLength + str2.stringLength);
		//
		size_t i = 0;
		//
		for (i = 0; i < str1.stringLength; i++)
		{
			returnString.stringData[i] = str1.stringData[i];
		}
		//
		for (; i < returnString.stringLength; i++)
		{
			returnString.stringData[i] = str2.stringData[i - str1.stringLength];
		}
		//
		return std::move(returnString);
	}
	//
	template<typename T>
	void BasicString<T>::Append(BasicString<T>& str1, const BasicStringView<T>& str2)
	{
		size_t previousLength = str1.stringLength;
		str1.Allocate(previousLength + str2.stringLength);
		//
		for (size_t i = previousLength; i < str1.stringLength; i++)
		{
			str1.stringData[i] = str2.stringData[i - previousLength];
		}
	}
	//
	template<typename T>
	void BasicString<T>::Append(BasicString<T>& str1, const char c)
	{
		size_t previousLength = str1.stringLength;
		str1.Allocate(str1.stringLength + 1);

		str1.stringData[previousLength] = c;
	}
	//
	template<typename T>
	SV::Array<SV::BasicString<T>> BasicString<T>::Split(const BasicStringView<T>& stringIn, const BasicStringView<T>& delimiter)
	{
		if (delimiter.stringLength > stringIn.stringLength || delimiter.stringLength <= 0)
		{
			return {};
		}

		SV::Array<BasicString<T>> strings;
		SV::Array<size_t> table(delimiter.stringLength);
		BasicStringView<T>::CalculateFindTable(delimiter, table);

		size_t j = BasicStringView<T>::Find(stringIn, delimiter, table, 0ui32);
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
				strings.Emplace(BasicStringView<T>::Substring(stringIn, i, j - i));
			}
			j += delimiter.stringLength;
			i = j;
			j = BasicStringView<T>::Find(stringIn, delimiter, table, j);
		}

		if (i != IndexUndefined && i < stringIn.stringLength)
		{
			strings.Emplace(BasicStringView<T>::Substring(stringIn, i, stringIn.stringLength - i));
		}

		return std::move(strings);
	}
	//
	template<typename T>
	size_t BasicString<T>::GetWordCount(const BasicStringView<T>& stringIn)
	{
		if (!stringIn.IsEmpty()) return IndexUndefined;
		//
		SV::Array<size_t> indicies = FindAll(stringIn, (const T)' ');
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
	template<typename T>
	SV::Array<SV::BasicString<T>> BasicString<T>::GetWords(const BasicStringView<T>& stringIn)
	{
		return std::move(BasicString<T>::Split(stringIn, (const T)' '));
	}
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Replace(const BasicStringView<T>& string, const BasicStringView<T>& oldString, const BasicStringView<T>& newString)
	{
		BasicString<T> returnString = string;
		ReplaceAssign(returnString, oldString, newString);
		return std::move(returnString);
	}
	//
	template<typename T>
	void BasicString<T>::ReplaceAssign(BasicString<T>& stringIn, const BasicStringView<T>& oldString, const BasicStringView<T>& newString)
	{
		if (stringIn.IsEmpty() && oldString.IsEmpty())
		{
			SV::Array<size_t> indicies = BasicStringView<T>::FindAll(stringIn, oldString);
			if (indicies[0] != IndexUndefined)
			{
				size_t j = 0;
				size_t k = 0;
				size_t o = 0;
				SV::Array<T> buffer(stringIn.stringLength);
				for (; j < stringIn.stringLength; j++)
				{
					buffer[j] = stringIn.stringData[j];
				}
				j = 0;

				long long newStringSize = stringIn.stringLength + (long long(indicies.GetCount() * newString.stringLength) - long long(indicies.GetCount() * oldString.stringLength));
				stringIn.Allocate((size_t)newStringSize);
				//stringIn.stringLength += (indicies.GetCount() * newString.stringLength) - (indicies.GetCount() * oldString.stringLength);
				//	T* newStringData = (T*)realloc(stringIn.stringData, stringIn.stringLength + 1);
				//if (newStringData == nullptr)
				//{
				//	SV_FATAL_T(BasicString<T>::ReplaceAssign(String&, const SV::StringView&, const SV::StringView&), "Memory could not be allocated for string.");
				//	throw std::overflow_error("Memory could not be allocated for string.");
				//}
				//stringIn.stringData = newStringData;

				for (size_t i = 0; i < stringIn.stringLength; i++)
				{
					if (k >= indicies.GetCount())
					{
						goto CopyFromBuffer;
					}
					else if (j == indicies[k])
					{
						for (o = 0; o < newString.stringLength; o++)
						{
							stringIn.stringData[i + o] = newString.stringData[o];
						}
						i += o - 1;
						j = indicies[k] + oldString.stringLength;
						k++;
					}
					else
					{
					CopyFromBuffer:
						stringIn.stringData[i] = buffer[j];
						j++;
					}
				}
			}
		}
	}
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Remove(const BasicStringView<T>& string, const BasicStringView<T>& oldString)
	{
		BasicString<T> returnString = string;
		ReplaceAssign(returnString, oldString, BasicStringView<T>::EmptyString);
		return std::move(returnString);
	}
	//
	template<typename T>
	void BasicString<T>::RemoveAssign(BasicString<T>& string, const BasicStringView<T>& oldString)
	{
		ReplaceAssign(string, oldString, BasicStringView<T>::EmptyString);
	}
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Join(const SV::Array<BasicStringView<T>>& strings)
	{
		return std::move(Join(BasicStringView<T>::EmptyString, strings));
	}
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Join(const BasicStringView<T>& joinDelimiter, const SV::Array<BasicStringView<T>>& strings)
	{
		size_t i = 0;
		size_t newSize = 0;
		for (i = 0; i < strings.GetCount(); i++)
		{
			newSize += strings[i].stringLength;
		}

		newSize += (joinDelimiter.stringLength * (strings.GetCount() - 1));

		BasicString<T> returnString(newSize);

		size_t currentLoc = 0;

		for (i = 0; i < strings.GetCount() - 1; i++)
		{
			memcpy(returnString.stringData + currentLoc, strings[i].stringData, strings[i].stringLength);
			currentLoc += strings[i].stringLength;
			memcpy(returnString.stringData + currentLoc, joinDelimiter.stringData, joinDelimiter.stringLength);
			currentLoc += joinDelimiter.stringLength;
		}

		memcpy(returnString.stringData + currentLoc, strings.Last().stringData, strings.Last().stringLength);

		return std::move(returnString);
	}
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Insert(const BasicStringView<T>& string, const BasicStringView<T>& stringToInsert, size_t index)
	{
		if (index < string.stringLength + 1)
		{
			BasicString<T> returnString(string.stringLength + stringToInsert.stringLength);
			memcpy(returnString.stringData, string.stringData, index);
			memcpy(returnString.stringData + index, stringToInsert.stringData, stringToInsert.stringLength);
			memcpy(returnString.stringData + index + stringToInsert.stringLength, string.stringData + index, string.stringLength - index);
			return std::move(returnString);
		}
		else
		{
			return (const T)' ';
		}
	}
	//
	template<typename T>
	void BasicString<T>::InsertAssign(BasicString<T>& string, const BasicStringView<T>& stringToInsert, size_t index)
	{
		if (index < string.stringLength + 1)
		{
			size_t originalLength = string.stringLength;
			string.Allocate(string.stringLength + stringToInsert.stringLength);
			char* buffer = (char*)malloc(originalLength - index);

			memcpy(buffer, string.stringData + index, originalLength - index);
			memcpy(string.stringData + index, stringToInsert.stringData, stringToInsert.stringLength);
			memcpy(string.stringData + index + stringToInsert.stringLength, buffer, originalLength - index);
		}
	}
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Splice(const BasicStringView<T>& string, size_t index, size_t length)
	{
		BasicString<T> returnString = string;
		SpliceAssign(returnString, index, length);
		return std::move(returnString);
	}
	//
	template<typename T>
	void BasicString<T>::SpliceAssign(BasicString<T>& string, size_t index, size_t length)
	{
		if (length + index <= string.stringLength)
		{
			string.operator=(std::move(BasicStringView<T>::Substring(string, 0, index) + BasicStringView<T>::Substring(string, index + length, string.stringLength - (index + length))));
		}
	}
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Reverse(const BasicStringView<T>& string)
	{
		BasicString<T> newString;
		newString.Allocate(string.stringLength);
		//
		size_t i = string.stringLength;
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
	template<typename T>
	void BasicString<T>::ReverseAssign(BasicString<T>& string)
	{
		SV::Array<T> newString(string.stringLength);
		//
		size_t i = string.stringLength;
		size_t j = 0;
		//
		for (; i > 0; i--)
		{
			newString[j] = string.stringData[i - 1];
			j++;
		}
		//
		for (i = 0; i < string.stringLength; i++)
		{
			string.stringData[i] = newString[i];
		}
	}
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::ToUpper(const BasicStringView<T>& str)
	{
		BasicString<T> returnString = str;
		ToUpperAssign(returnString);
		return std::move(returnString);
	}
	//
	template<typename T>
	void BasicString<T>::ToUpperAssign(BasicString<T>& string)
	{
		for (size_t i = 0; i < string.stringLength; i++)
		{
			string.stringData[i] = BasicStringView<T>::CharToUpper(string.stringData[i]);
		}
	}
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::ToLower(const BasicStringView<T>& str)
	{
		BasicString<T> returnString = str;
		ToLowerAssign(returnString);
		return std::move(returnString);
	}
	//
	template<typename T>
	void BasicString<T>::ToLowerAssign(BasicString<T>& string)
	{
		for (size_t i = 0; i < string.stringLength; i++)
		{
			string.stringData[i] = BasicStringView<T>::CharToLower(string.stringData[i]);
		}
	}

#pragma endregion

#pragma region Instantiated String Operations
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Concatenate(const BasicStringView<T>& str1) const { return Concatenate((*this), str1); }
	//
	template<typename T>
	void BasicString<T>::Append(const BasicStringView<T>& str1) { Append(*this, str1); }
	template<typename T>
	void BasicString<T>::Append(const char c) { Append(*this, c); }
	//
	template<typename T>
	SV::Array<SV::BasicString<T>> BasicString<T>::Split(const BasicStringView<T>& delimiter) const { return std::move(Split((*this), delimiter)); }
	//
	template<typename T>
	size_t BasicString<T>::GetWordCount() const { return GetWordCount((*this)); }
	//
	template<typename T>
	SV::Array<SV::BasicString<T>> BasicString<T>::GetWords() const { return std::move(GetWords((*this))); }
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Replace(const BasicStringView<T>& oldString, const BasicStringView<T>& newString) const { return Replace((*this), oldString, newString); }
	//
	template<typename T>
	void BasicString<T>::ReplaceAssign(const BasicStringView<T>& oldString, const BasicStringView<T>& newString) { ReplaceAssign(*this, oldString, newString); }
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Remove(const BasicStringView<T>& oldString) const { return Remove((*this), oldString); }
	//
	template<typename T>
	void BasicString<T>::RemoveAssign(const BasicStringView<T>& oldString) { RemoveAssign(*this, oldString); }
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Insert(const BasicStringView<T>& stringToInsert, size_t index) const { return Insert((*this), stringToInsert, index); }
	//
	template<typename T>
	void BasicString<T>::InsertAssign(const BasicStringView<T>& stringToInsert, size_t index) { InsertAssign(*this, stringToInsert, index); }
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Splice(size_t index, size_t length) const { return Splice((*this), index, length); }
	//
	template<typename T>
	void BasicString<T>::SpliceAssign(size_t index, size_t length) { SpliceAssign(*this, index, length); }
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::Reverse() const { return Reverse((*this)); }
	//
	template<typename T>
	void BasicString<T>::ReverseAssign() { ReverseAssign(*this); }
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::ToUpper() const { return ToUpper((*this)); }
	//
	template<typename T>
	void BasicString<T>::ToUpperAssign() { ToUpperAssign(*this); }
	//
	template<typename T>
	SV::BasicString<T> BasicString<T>::ToLower() const { return ToLower((*this)); }
	//
	template<typename T>
	void BasicString<T>::ToLowerAssign() { ToLowerAssign(*this); }

#pragma endregion

#pragma region Operator Overloads

	template<typename T>
	BasicString<T>& BasicString<T>::operator=(const SV::BasicString<T>& string)
	{
		this->Allocate(string.stringLength);
		memcpy(this->stringData, string.stringData, string.stringLength * sizeof(T));
		return *this;
	}
	template<typename T>
	BasicString<T>& BasicString<T>::operator=(const BasicStringView<T>& string)
	{
		this->Allocate(string.stringLength);
		memcpy(this->stringData, string.stringData, this->stringLength * sizeof(T));
		return *this;
	}
	template<typename T>
	BasicString<T>& BasicString<T>::operator=(const T* string)
	{
		this->Allocate(MyStringView::GetCStringLength(string));
		memcpy(this->stringData, string, this->stringLength * sizeof(T));
		return *this;
	}
	template<typename T>
	BasicString<T>& BasicString<T>::operator=(const std::basic_string<T>& string)
	{
		Allocate(string.size());
		memcpy(this->stringData, string.c_str(), string.size() * sizeof(T));
		return *this;
	}
	template<typename T>
	BasicString<T>& BasicString<T>::operator=(const std::basic_stringstream<T>& ss) noexcept
	{
		this->operator=(ss.str());
		return *this;
	}
	//
	template<typename T>
	BasicString<T>& BasicString<T>::operator=(SV::BasicString<T>&& string) noexcept
	{
		if (this != &string)
		{
			if (string.allocatorStore == AllocatorStore::SSOptimise)
			{
				if (this->allocatorStore == AllocatorStore::Heap && this->stringData != this->charStore) free((void*)this->stringData);
				this->stringData = &this->optimiseStore[0];
				this->allocatorStore = AllocatorStore::SSOptimise;
				this->stringLength = string.stringLength;
				this->stringCapacity = STR_OPTIMISE_STORE - 1;

				memcpy(this->stringData, string.stringData, (string.stringLength + 1) * sizeof(T));
			}
			else
			{
				this->stringData = string.stringData;
				this->stringLength = string.stringLength;
				this->stringCapacity = string.stringCapacity;
				this->allocatorStore = string.allocatorStore;
			}

			string.stringData = nullptr;
			string.stringLength = 0;
			string.stringCapacity = 0;
		}
		return *this;
	}
	//
	template<typename T>
	BasicString<T>& BasicString<T>::operator+=(const BasicStringView<T>& str1)
	{
		this->Append(str1);
		return *this;
	}
	template<typename T>
	BasicString<T>& BasicString<T>::operator+=(const BasicString<T>& str1)
	{
		this->Append(str1);
		return *this;
	}
	template<typename T>
	BasicString<T>& BasicString<T>::operator+=(const T* str1)
	{
		this->Append((BasicStringView<T>)str1);
		return *this;
	}
	//
	template<typename T>
	BasicString<T>::operator T* () { return this->stringData; }

#pragma endregion

#pragma region Miscellaneous Functions

	template<typename T>
	void BasicString<T>::Clear(const bool freeMemory)
	{
		if (this->stringData == this->charStore ||
			this->stringData == nullptr) return;

		if (freeMemory && this->allocatorStore == AllocatorStore::Heap)
		{
			free((void*)this->stringData);

			this->stringData = &this->optimiseStore[0];
			this->allocatorStore = AllocatorStore::SSOptimise;
			this->stringCapacity = STR_OPTIMISE_STORE - 1;
		}

		this->stringLength = 0;
		this->stringData[0] = 0;
	}

	template<typename T>
	void BasicString<T>::TrimToCapacity()
	{
		if (this->allocatorStore == AllocatorStore::SSOptimise ||
			this->stringData == this->charStore ||
			this->stringCapacity == this->stringLength ||
			this->stringData == nullptr) return;

		this->stringData = (T*)realloc(this->stringData, (this->stringLength + 1) * sizeof(T));
		this->stringCapacity = this->stringLength;
	}

#pragma endregion

#pragma region Mutable Array Access Functions
	//
	template<typename T>
	T& BasicString<T>::operator[](size_t index)
	{
		SV_ASSERT_T(index + 1 > this->stringLength, BasicString<T>::operator[], "Index out of bounds");
		return (T&)this->stringData[index];
	}
	//
	template<typename T>
	T& BasicString<T>::Get(size_t index)
	{
		SV_ASSERT_T(index + 1 > this->stringLength, BasicString<T>::Get, "Index out of bounds");
		return (T&)this->stringData[index];
	}
	//
	template<typename T>
	void BasicString<T>::Set(size_t index, T c)
	{
		SV_ASSERT_T(index + 1 > this->stringLength, BasicString<T>::Set, "Index out of bounds");
		this->stringData[index] = c;
	}
	//
	template<typename T>
	T& BasicString<T>::First()
	{
		return this->stringData[0];
	}
	//
	template<typename T>
	T& BasicString<T>::Last()
	{
		return this->stringData[this->stringLength];
	}
	//
	template<typename T>
	T* BasicString<T>::begin() { return &this->stringData[0]; }
	template<typename T>
	T* BasicString<T>::end() { return &this->stringData[stringLength]; }
	//
#pragma endregion

#pragma endregion
	//
}
