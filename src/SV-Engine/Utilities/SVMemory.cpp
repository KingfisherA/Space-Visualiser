#include"SV-Engine/Precompiled Global.hpp"
#include"SVMemory.hpp"

//
//
void SV::ClearMemoryRegion(void* region, size_t byteSize)
{
	if (region == nullptr) return;
	for (unsigned int i = 0; i < byteSize; i++)
	{
		*(reinterpret_cast<char*>(region) + i) = 0;
	}
}
//