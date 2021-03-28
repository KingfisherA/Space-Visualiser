#pragma once
#include<exception>

namespace SV
{
	template <class Arg1, class Arg2> class ComparisonFunction
	{
	public:
		//
		ComparisonFunction() {}
		//
		ComparisonFunction(bool(*callback)(Arg1, Arg2))
		{
			function = callback;
		}
		//
		ComparisonFunction(ComparisonFunction<Arg1, Arg2>& n)
		{
			function = n.function;
		}
		//
		inline bool Call(Arg1 a1, Arg2 a2)
		{
			if (function != nullptr)
			{
				return function(a1, a2);
			}
			else
			{
				throw std::exception("Callback function is nullptr!");
				return false;
			}
		}
		//
		void SetCallbackFunction(bool(*callback)(Arg1, Arg2))
		{
			function = callback;
		}
		//
		void operator+=(bool(*callback)(Arg1, Arg2))
		{
			function = callback;
		}
		//
		void operator=(bool(*callback)(Arg1, Arg2))
		{
			function = callback;
		}
		//
		void operator=(ComparisonFunction<Arg1, Arg2>& n)
		{
			function = n.function;
		}
		//
		bool(*GetFunctionPointer())(Arg1, Arg2)
		{
			return function;
		}
		//
	private:
		bool(*function)(Arg1, Arg2) = nullptr;
	};
}