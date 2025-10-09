#pragma once
#include<chrono>

namespace SV
{
	class PerformanceTimer
	{
	public:
		void StartTimer()
		{
			timePoint1 = std::chrono::high_resolution_clock::now();
		}
		void StopTimer()
		{
			timePoint2 = std::chrono::high_resolution_clock::now();
		}
		template<typename A>
		unsigned long long StopTimer()
		{
			timePoint2 = std::chrono::high_resolution_clock::now();
			return std::chrono::duration_cast<A>(timePoint2 - timePoint1).count();
		}
		template<typename A>
		unsigned long long GetResult()
		{
			return std::chrono::duration_cast<A>(timePoint2 - timePoint1).count();
		}
	private:
		std::chrono::steady_clock::time_point timePoint1;
		std::chrono::steady_clock::time_point timePoint2;
	};
};