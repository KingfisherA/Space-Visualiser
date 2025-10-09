#pragma once
#include<iostream>
#include<initializer_list>
#include<type_traits>
#include<chrono>
#include<limits>
//
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
using namespace glm;

//Fix using templates
#ifdef _WIN64
constexpr unsigned long long IndexUndefined = 0xffffffffffffffff;
#else
constexpr unsigned int IndexUndefined = 0xffffffff;
#endif // _WIN64

template<typename T, typename... Others>
constexpr bool is_any_of_v = (std::is_same_v<T, Others> || ...);

namespace SV
{
	template<typename T> class BasicStringView;
	template<typename T> class BasicString;

	using StringView = BasicStringView<char>;
	using String = BasicString<char>;

	template<typename A>
	using Seconds = std::chrono::duration<A, std::ratio<1, 1>>;
	template<typename A>
	using Deciseconds = std::chrono::duration<A, std::deci>;
	template<typename A>
	using Milliseconds = std::chrono::duration<A, std::milli>;
	template<typename A>
	using Microseconds = std::chrono::duration<A, std::micro>;
	template<typename A>
	using Nanoseconds = std::chrono::duration<A, std::nano>;
	template<typename A>
	using Picoseconds = std::chrono::duration<A, std::pico>;
	template<typename A>
	using Femtoseconds = std::chrono::duration<A, std::femto>;
	template<typename A>
	using Attoseconds = std::chrono::duration<A, std::atto>;

	template<typename T> struct IsRef : public std::false_type {};
	template<typename T> struct IsRef<std::shared_ptr<T>> : public std::true_type {};

	template<typename Type>
	constexpr bool IsCharacter = is_any_of_v<std::decay_t<Type>, char, signed char, unsigned char, wchar_t>;
// #ifdef __cpp_char8_t
// 		char8_t,
// #endif // __cpp_char8_t 
// 		char16_t, char32_t>;

	//
#pragma region SVUtilities Macros
//
#define SymbolName(lexarSymbol) #lexarSymbol
//
#pragma endregion
//

	//
	class Utilities final
	{
	public:
		//
		static SV::String ReadFile(const SV::StringView filename);
		//
		static bool IsExtensionCompatible(const StringView& extension);
		//
	};

	//
	class Random
	{
		template<typename Number>
		static Number GetRandom()
		{
			static_assert(std::is_integral_v<Number> || std::is_floating_point_v<Number>, "Template parameter \"Number\" is not a number type.");
			if (std::is_floating_point_v<Number>)
			{
				return static_cast <Number>(rand()) / static_cast <Number>(RAND_MAX);
			}
			else
			{
				return static_cast<Number>(rand());
			}
		}
	};

}

namespace std
{
	template<> class hash<std::pair<unsigned int, unsigned int>>
	{
	public:
		size_t operator()(const std::pair<unsigned int, unsigned int>& pairVal) const
		{
			std::hash<unsigned int> hasher;
			return  hasher(pairVal.first) + hasher(pairVal.second);
		}
	};
}
