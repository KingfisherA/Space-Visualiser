#pragma once
#include"SV/Precompiled Global.h"

//Create a new object in a shared pointer 
template<typename T> using Ref = std::shared_ptr<T>;
template<typename T> using uRef = std::unique_ptr<T>;

using std::move;

template<typename T> class WeakRef : public std::shared_ptr<T>
{
public:
	WeakRef(T* ptr) : std::shared_ptr<T>(ptr, [](T*) {}) {}
	WeakRef(T& ptr) : std::shared_ptr<T>(&ptr, [](T*) {}) {}
};

//Create a new object in a shared pointer
template<typename T> inline Ref<T> create()
{
	return std::make_shared<T>();
}
//Create a new object in a shared pointer with constructor arguments
template<typename T, typename... Types> inline Ref<T> create(Types&&... arguments)
{
	return std::make_shared<T>(arguments...);
}
//Create a new object in a shared pointer
template<typename T> inline Ref<T> make()
{
	return std::make_shared<T>();
}
//Create a new object in a shared pointer with constructor arguments
template<typename T, typename... Types> inline Ref<T> make(Types&&... arguments)
{
	return std::make_shared<T>(arguments...);
}
//Create a new object in a unique pointer
template<typename T> inline uRef<T> umake()
{
	return std::make_unique<T>();
}
//Create a new object in a unique pointer with constructor arguments
template<typename T, typename... Types> inline uRef<T> umake(Types&&... arguments)
{
	return std::make_unique<T>(arguments...);
}
//Get raw pointer
#define refptr .operator->()
//Reset a pointer type
#define resetref(ptrX, ...) ptrX.reset(__VA_ARGS__)

namespace SV
{
	void ClearMemoryRegion(void* region, size_t byteSize);
}