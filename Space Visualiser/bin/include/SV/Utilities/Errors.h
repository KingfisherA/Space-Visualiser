#pragma once
#include<exception>
#include<stdio.h>
#include<iostream>

#include"SV/Utilities/SVUtilities.h"
#include"SV/Utilities/Console.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)


#if _ITERATOR_DEBUG_LEVEL == 2 || _ITERATOR_DEBUG_LEVEL == 1
#define SV_DEBUG
#else
#define SV_RELEASE
#endif

#ifdef SV_DEBUG
#define SV_ENABLE_ASSERTS
#endif

#define SV_ASSERT_WARN(x, ...) { if(x) { SV::Console::SetConsoleColour(SV::ConsoleColour::Yellow); printf("Warning: %s", __VA_ARGS__); SV::Console::NewLine();} }
#define SV_ASSERT_WARN_T(x, y, ...) { if(x) { SV::Console::SetConsoleColour(SV::ConsoleColour::Yellow); printf("[%s] Warning: %s", TOSTRING(y),  __VA_ARGS__); SV::Console::NewLine(); } }

#define SV_WARN(...) {  SV::Console::SetConsoleColour(SV::ConsoleColour::Yellow); printf("Warning: %s", __VA_ARGS__); SV::Console::NewLine(); }
#define SV_WARN_T(y, ...) { SV::Console::SetConsoleColour(SV::ConsoleColour::Yellow); printf("[%s] Warning: %s", TOSTRING(y),  __VA_ARGS__); SV::Console::NewLine();}

#ifdef SV_ENABLE_ASSERTS
#define SV_ASSERT(x, ...) { if(x) { SV::Console::SetConsoleColour(SV::ConsoleColour::BrightRed); printf("Assertion Failed: %s", __VA_ARGS__);  SV::Console::NewLine();__debugbreak(); exit(0);} }
#define SV_ASSERT_T(x, y, ...) { if(x) { SV::Console::SetConsoleColour(SV::ConsoleColour::BrightRed); printf("[%s] Assertion Failed: %s", TOSTRING(y),  __VA_ARGS__); SV::Console::NewLine(); __debugbreak(); exit(0);} }

#define SV_FATAL(...) {  SV::Console::SetConsoleColour(SV::ConsoleColour::BrightRed); printf("Fatal Error Occured: %s", __VA_ARGS__); SV::Console::NewLine(); __debugbreak(); exit(0);}
#define SV_FATAL_T(y, ...) { SV::Console::SetConsoleColour(SV::ConsoleColour::BrightRed); printf("[%s] Fatal Error Occured: %s", TOSTRING(y),  __VA_ARGS__); SV::Console::NewLine(); __debugbreak(); exit(0);}

#define SV_THROW_RELEASE(x)

#define SV_FATAL_THROW(type, except, ...) SV_FATAL_T(type, __VA_ARGS__)

#else
#define SV_ASSERT(x,...)
#define SV_ASSERT_T(x, y, ...)

#define SV_FATAL(...)
#define SV_FATAL_T(y, ...)

#define SV_THROW_RELEASE(x) throw x;

#define SV_FATAL_THROW(type, except, ...) throw except(__VA_ARGS__)

#endif
//
//namespace SV
//{
//	template <class T> class Expected
//	{
//		union
//		{
//			T value;
//			std::exception_ptr exceptionPtr;
//		};
//		bool isValid = false;
//		Expected() {} // used internally
//
//	public:
//		//
//		Expected(const T& rhs) : value(rhs), isValid(true) {}
//		//
//		Expected(T&& rhs)
//			: value(std::move(rhs))
//			, isValid(true) {}
//		//
//		Expected(const Expected& rhs) : isValid(rhs.isValid)
//		{
//			if (isValid) new(&value) T(rhs.value);
//			else new(&exceptionPtr) std::exception_ptr(rhs.exceptionPtr);
//		}
//		//
//		Expected(Expected&& rhs) : isValid(rhs.isValid)
//		{
//			if (isValid) new(&value) T(std::move(rhs.value));
//			else new(&exceptionPtr)
//				std::exception_ptr(std::move(rhs.exceptionPtr));
//		}
//		//
//		void Swap(Expected& rhs)
//		{
//			if (isValid)
//			{
//				if (rhs.isValid)
//				{
//					std::swap(value, rhs.value);
//				}
//				else
//				{
//					auto t = std::move(rhs.exceptionPtr);
//					new(&rhs.value) T(std::move(value));
//					new(&exceptionPtr) std::exception
//						_ptr(t);
//					std::swap(isValid, rhs.isValid);
//				}
//			}
//			else
//			{
//				if (rhs.isValid)
//				{
//					rhs.swap(*this);
//				}
//				else
//				{
//					exceptionPtr.swap(rhs.exceptionPtr);
//					std::swap(isValid, rhs.isValid);
//				}
//			}
//		}
//		//
//		template <class E>
//		static Expected<T> FromException(const E& exception)
//		{
//			if (typeid(exception) != typeid(E)) {
//				throw std::invalid_argument("Slicing detected");
//			}
//			return fromException(
//				std::make_exception_ptr(exception));
//		}
//		static Expected<T> FromException(std::exception_ptr p) {
//			Expected<T> result;
//			result.isValid = false;
//			new(&result.exceptionPtr) std::exception_ptr(std::move(p));
//			return result;
//		}
//		static Expected<T> FromException()
//		{
//			return FromException(std::current_exception());
//		}
//		//
//		bool IsValid() const
//		{
//			return isValid;
//		}
//		T& Get()
//		{
//			if (!isValid) std::rethrow_exception(exceptionPtr);
//			return value;
//		}
//		const T& Get() const
//		{
//			if (!isValid) std::rethrow_exception(exceptionPtr);
//			return value;
//		}
//		//
//		template <class E>
//		bool HasException() const
//		{
//			try
//			{
//				if (!isValid) std::rethrow
//					_exception(exceptionPtr);
//			}
//			catch
//				(const E& object) {
//				return true;
//			}
//			catch (...) {
//			}
//			return false;
//		}
//		//
//		template <class F>
//		static Expected FromCode(F fun)
//		{
//			try {
//				return Expected(fun());
//			}
//			catch (...) {
//				return fromException();
//			}
//		}
//		//
//	};
//}
