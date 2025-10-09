#include"Array.hpp"
#include"SV-Engine/Utilities/SVUtilities.hpp"


//
//
namespace SV
{

#pragma region Array Base Implementations

	//Logical Operators
	template<typename A>
	 bool ArrayBase<A>::operator==(const ArrayBase<A>& arr) const
	{
		if (this->count != arr.count) return false;

		for (size_t i = 0; i < this->count * sizeof(A); i++)
		{
			if (reinterpret_cast<char*>(this->internalArray)[i] != reinterpret_cast<char*>(arr.internalArray)[i]) return true;
		}

		return false;
	}
	template<typename A>
	 bool ArrayBase<A>::operator!=(const ArrayBase<A>& arr) const
	{
		if (this->count != arr.count) return true;

		for (size_t i = 0; i < this->count * sizeof(A); i++)
		{
			if (reinterpret_cast<char*>(this->internalArray)[i] != reinterpret_cast<char*>(arr.internalArray)[i]) return false;
		}

		return true;
	}

	//Get Functions
	template<typename A>
	 A& ArrayBase<A>::operator[] (const size_t index)
	{
		SV_ASSERT_T(index + 1 > this->count, SV::ArrayBase<A>::operator[], "Index is greater than array this->count");

		return internalArray[index];
	}
	template<typename A>
	 const A& ArrayBase<A>::operator[] (const size_t index) const
	{
		SV_ASSERT_T(index + 1 > this->count, SV::ArrayBase<A>::operator[], "Index is greater than array this->count");

		return (const A&)internalArray[index];
	}
	template<typename A>
	 const A& ArrayBase<A>::Get(size_t index) const
	{
		SV_ASSERT_T(index + 1 > this->count, SV::Array<A>::Get() const, "Index is greater than array this->count");
		return internalArray[index];
	}
	template<typename A>
	 A& ArrayBase<A>::Get(size_t index)
	{
		SV_ASSERT_T(index + 1 > this->count, SV::Array<A>::Get(), "Index is greater than array this->count")

			return internalArray[index];
	}
	template<typename A>
	 A* ArrayBase<A>::GetPtr(size_t index)
	{
		SV_ASSERT_T(index + 1 > this->count, SV::Array<A>::GetPtr(), "Index is greater than array this->count")

			return (A*)&internalArray[index];
	}
	template<typename A>
	SV::Array<A> ArrayBase<A>::GetRange(size_t startIndex, size_t endIndex) const
	{
		SV_ASSERT_T(startIndex + 1 > this->count, SV::Array<A>::GetRange(), "Start index is greater than array this->count");
		SV_ASSERT_T(endIndex + 1 > this->count, SV::Array<A>::GetRange(), "End index is greater than array this->count");
		SV_ASSERT_T(startIndex > endIndex, SV::Array<A>::GetRange(), "Start index cannot be greater than end index");

		SV::Array<A> objects((endIndex - startIndex) + 1, true);
		objects.AddRange(*this, startIndex, endIndex);

		return static_cast<SV::Array<A>&&>(objects);
	}

	template<typename A>
	template<typename Type, bool useCache>
	size_t ArrayBase<A>::GetInstanceIndex() const
	{
		size_t hashCode = 0;
		if constexpr (useCache)
		{
			if (typeCachePtr == nullptr) typeCachePtr = make <ABTypeCache>();

			hashCode = typeid(Type).hash_code();
			if (const auto kvPair = typeCachePtr->find(hashCode); kvPair != typeCachePtr->end())
			{
				return kvPair->second;
			}
		}

		for (size_t i = 0; i < this->count; i++)
		{
			Type* ptr = nullptr;

			if constexpr (SV::IsRef<A>::value == true)
			{
				ptr = std::dynamic_pointer_cast<Type>(this->internalArray[i]).operator->();
			}
			else
			{
				ptr = dynamic_cast<Type*>(&this->internalArray[i]);
			}

			if (ptr != nullptr)
			{
				if constexpr (useCache) typeCachePtr->insert({ hashCode, i });
				return i;
			}
		}

		return IndexUndefined;
	}
	template<typename A>
	template<typename Type, bool useCache>
	const Type* ArrayBase<A>::GetInstance() const
	{
		unsigned int index = this->GetInstanceIndex<Type, useCache>();
		if constexpr (SV::IsRef<A>::value == true)
		{
			return index == IndexUndefined ? nullptr : (const Ref<Type>*) & this->internalArray[index];
		}
		else
		{
			return index == IndexUndefined ? nullptr : (const Type*)&this->internalArray[index];
		}
	}
	template<typename A>
	template<typename Type, bool useCache>
	Type* ArrayBase<A>::GetInstance()
	{
		unsigned int index = this->GetInstanceIndex<Type, useCache>();
		return index == IndexUndefined ? nullptr : (Type*)&this->internalArray[index];
	}
	template<typename A>
	template<typename Type>
	SV::Array<size_t> ArrayBase<A>::GetAllInstanceIndicies() const
	{
		SV::Array<size_t> indicies;
		for (size_t i = 0; i < this->count; i++)
		{
			Type* ptr = nullptr;
			if constexpr (SV::IsRef<A>::value == true)
			{
				ptr = std::dynamic_pointer_cast<Type>(this->internalArray[i]).operator->();
			}
			else
			{
				ptr = dynamic_cast<Type*>(&this->internalArray[i]);
			}

			if (ptr != nullptr)
			{
				indicies.Add(i);
			}
		}

		return std::move(indicies);
	}
	template<typename A>
	template<typename Type>
	 void ArrayBase<A>::GetAllInstances(SV::Array<Type*>& instances) const
	{
		const SV::Array<size_t>& indicies = GetAllInstanceIndicies<Type>();
		for (const size_t i : indicies)
		{
			instances.Add((Type*)&this->internalArray[i]);
		}
	}
	template<typename A>
	void ArrayBase<A>::ClearTypeCache() const
	{
		if (typeCachePtr != nullptr) typeCachePtr->clear();
	}

	//Set Functions
	template<typename A>
	 void ArrayBase<A>::Set(size_t index, const A& obj)
	{
		static_assert(std::is_copy_assignable_v<A>, "To set an element, a valid copy assign operator= must be provided");
		SV_ASSERT_T(index + 1 > this->count, SV::ArrayBase<A>::Set, "Index cannot be greater than array this->count");
		internalArray[index] = obj;
	}
	template<typename A>
	void ArrayBase<A>::SetRange(size_t start, const std::initializer_list<A>& objs)
	{
		return this->SetRange(start, ArrayView<A>((A*)objs.begin(), 0, objs.size()));
	}
	template<typename A>
	 void ArrayBase<A>::SetRange(size_t start, const SV::ArrayView<A>& objs)
	{
		SV_ASSERT_T(start + 1 > this->count, SV::ArrayBase<A>::SetRange, "Start index cannot be greater than the array this->count + 1");
		SV_ASSERT_T(start + objs.GetCount() + 1 > this->count, SV::ArrayBase<A>::SetRange, "Cannot write data as it will overflow array bounds");

		static_assert(std::is_copy_assignable_v<A>, "To set elements, a valid copy assign operator= must be provided");

		for (size_t i = 0; i < objs.GetCount(); i++)
		{
			this->internalArray[i + start] = objs.GetInternalArray()[i];
		}
	}
	template<typename A>
	 void ArrayBase<A>::SetByteCopy(size_t index, const A& obj)
	{
		SV_ASSERT_T(index + 1 > this->count, SV::ArrayBase<A>::SetByteCopy, "Index is greater than array this->count");

		memcpy(&internalArray[index], &obj, sizeof(A));
	}

	//Move and Swap Functions
	template<typename A>
	 void ArrayBase<A>::Move(size_t index, size_t newIndex)
	{
		SV_ASSERT_T(index + 1 > this->count, SV::Array<A>::Move, "Index cannot be greater than this->count");
		SV_ASSERT_T(newIndex + 1 > this->count, SV::Array<A>::Move, "New index cannot be greater than this->count");
		if (index == newIndex) return;

		unsigned char* tempObj = (unsigned char*)alloca(sizeof(A));
		memcpy(tempObj, internalArray + index, sizeof(A));

		unsigned char* indexPtr = reinterpret_cast<unsigned char*>(internalArray + index);
		unsigned char* endPtr = reinterpret_cast<unsigned char*>(internalArray + this->count - 1);

		for (; indexPtr < endPtr; indexPtr++)
		{
			(*indexPtr) = *(indexPtr + sizeof(A));
		}

		memcpy(internalArray + newIndex, tempObj, sizeof(A));
	}
	template<typename A>
	 void ArrayBase<A>::Swap(size_t index1, size_t index2)
	{
		SV_ASSERT_T(index1 + 1 > this->count, SV::Array<A>::Swap, "Index 1 cannot be greater than this->count");
		SV_ASSERT_T(index2 + 1 > this->count, SV::Array<A>::Move, "Index 2 cannot be greater than this->count");
		if (index1 == index2) return;

		unsigned char* tempObj = (unsigned char*)alloca(sizeof(A));
		unsigned char*& heapArrayPtr = reinterpret_cast<unsigned char*&>(internalArray);

		size_t i = 0;
		for (i = 0; i < sizeof(A); i++)
		{
			tempObj[i] = heapArrayPtr[index1 * sizeof(A) + i];
		}

		for (i = 0; i < sizeof(A); i++)
		{
			heapArrayPtr[index1 * sizeof(A) + i] = heapArrayPtr[index2 * sizeof(A) + i];
		}

		for (i = 0; i < sizeof(A); i++)
		{
			heapArrayPtr[index2 * sizeof(A) + i] = tempObj[i];
		}

	}

	//Find Functions
	template<typename A>
	 size_t ArrayBase<A>::Find(const A& obj, size_t offset) const
	{
		if (offset > this->count - 1) return IndexUndefined;
		for (size_t i = offset; i < this->count; i++)
			if (internalArray[i] == obj) return i;
		return IndexUndefined;
	}
	template<typename A>
	 SV::Array<A> ArrayBase<A>::FindAll(const A& obj, size_t offset) const
	{
		Array<A> itemsArray;
		FindAll(obj, itemsArray, offset);
		return static_cast<SV::Array<A>&&>(itemsArray);
	}
	template<typename A>
	 void ArrayBase<A>::FindAll(const A& obj, SV::Array<int>& items, size_t offset) const
	{
		items.Clear();
		if (offset > this->count - 1)  return;

		for (size_t i = offset; i < this->count; i++)
			if (internalArray[i] == obj) items.Add(i);
	}

#pragma endregion

#pragma region Static Array Implementations
	//
	template<typename A, size_t myCount>
	 StaticArray<A, myCount>::StaticArray()
	{
		this->count = myCount;
		this->internalArray = (A*)&staticArray[0];

		if constexpr (std::is_default_constructible_v<A>)
		{
			for (size_t i = 0; i < myCount; i++)
			{
				::new((void*)(this->internalArray + i)) A();
			}
		}
	}
	template<typename A, size_t myCount>
	 StaticArray<A, myCount>::StaticArray(const std::initializer_list<A>& dataIn)
	{
		this->count = myCount;
		this->internalArray = (A*)&staticArray[0];
		memset(&this->staticArray[0], 0, myCount * sizeof(A));

		size_t minSize = min(myCount, dataIn.size());
		for (size_t i = 0; i < minSize; i++)
		{
			if constexpr (std::is_move_constructible_v<A>) ::new((void*)(this->internalArray + i)) A((A&&)dataIn.begin()[i]);
			else ::new((void*)(this->internalArray + i)) A((const A&)dataIn.begin()[i]);
		}
	}
	template<typename A, size_t myCount>
	 StaticArray<A, myCount>::StaticArray(const ArrayBase<A>& arrayIn)
	{
		this->count = myCount;
		this->internalArray = (A*)&staticArray[0];
		memset(&this->staticArray[0], 0, myCount * sizeof(A));

		size_t minSize = min(myCount, arrayIn.GetCount());
		for (size_t i = 0; i < minSize; i++)
		{
			::new((void*)(this->internalArray + i)) A(arrayIn[i]);
		}
	}
	template<typename A, size_t myCount>
	 StaticArray<A, myCount>::StaticArray(const ArrayView<A>& arrayIn)
	{
		this->count = myCount;
		this->internalArray = (A*)&staticArray[0];
		memset(&this->staticArray[0], 0, myCount * sizeof(A));

		size_t minSize = min(myCount, arrayIn.GetCount());
		for (size_t i = 0; i < minSize; i++)
		{
			::new((void*)(this->internalArray + i)) A(arrayIn[i]);
		}
	}
	template<typename A, size_t myCount>
	StaticArray<A, myCount>::StaticArray(const StaticArray<A, myCount>& arrayIn)
	{
		this->count = myCount;
		this->internalArray = (A*)&staticArray[0];
		memset(&this->staticArray[0], 0, myCount * sizeof(A));

		size_t minSize = min(myCount, arrayIn.GetCount());
		for (size_t i = 0; i < minSize; i++)
		{
			::new((void*)(this->internalArray + i)) A(arrayIn[i]);
		}
	}
	template<typename A, size_t myCount>
	StaticArray<A, myCount>::StaticArray(StaticArray<A, myCount>&& arrayIn) noexcept
	{
		this->count = myCount;
		this->internalArray = (A*)&staticArray[0];
		memset(&this->staticArray[0], 0, myCount * sizeof(A));

		size_t minSize = min(myCount, arrayIn.GetCount());
		for (size_t i = 0; i < minSize; i++)
		{
			if constexpr (std::is_move_constructible_v<A>) ::new((void*)(this->internalArray + i)) A((A&&)arrayIn[i]);
			else ::new((void*)(this->internalArray + i)) A((const A&)arrayIn[i]);
		}
	}
	template<typename A, size_t myCount>
	StaticArray<A, myCount>::~StaticArray() noexcept
	{
		if constexpr (std::is_trivially_destructible_v<A>) return;
		for (size_t i = 0; i < myCount; i++)
		{
			this->internalArray[i].~A();
		}
	}
	//
	template<typename A, size_t myCount>
	StaticArray<A, myCount>& StaticArray<A, myCount>::operator=(const StaticArray<A, myCount>& arrayIn)
	{
		if (this != &arrayIn)
		{
			size_t i = 0;
			if constexpr (!std::is_trivially_destructible_v<A>)
			{
				for (; i < myCount; i++)
				{
					this->internalArray[i].~A();
				}
			}

			size_t minSize = min(myCount, arrayIn.count);
			for (i = 0; i < myCount; i++)
			{
				::new((void*)(this->internalArray + i)) A((const A&)arrayIn[i]);
			}
		}
		return *this;
	}
	template<typename A, size_t myCount>
	StaticArray<A, myCount>& StaticArray<A, myCount>::operator=(StaticArray<A, myCount>&& arrayIn) noexcept
	{
		if (this != &arrayIn)
		{
			size_t i = 0;
			if constexpr (!std::is_trivially_destructible_v<A>)
			{
				for (; i < myCount; i++)
				{
					this->internalArray[i].~A();
				}
			}

			size_t minSize = min(myCount, arrayIn.count);
			for (i = 0; i < myCount; i++)
			{
				if constexpr (std::is_move_constructible_v<A>) ::new((void*)(this->internalArray + i)) A((A&&)arrayIn[i]);
				else ::new((void*)(this->internalArray + i)) A((const A&)arrayIn[i]);
			}
		}
		return *this;
	}
	//
#pragma endregion

#pragma region Array Implementations

	//Constructors and Destructors
	template<typename A>
	 Array<A>::Array()
	{
		Allocate(DEFAULT_ARRAY_CAPACITY);
	}
	template<typename A>
	 Array<A>::Array(size_t capacity, bool resize)
	{
		if (capacity == 0)
		{
			this->internalArray = nullptr;
			this->count = 0;
			this->capacity = 0;
			return;
		}

		if (resize)
		{
			Resize(capacity);
		}
		else
		{
			Allocate(capacity);
		}
	}
	template<typename A>
	 Array<A>::Array(const std::initializer_list<A>& elements)
	{
		if (elements.size() == 0)
		{
			Allocate(DEFAULT_ARRAY_CAPACITY);
			return;
		}
		Allocate(elements.size());
		AddRange(elements);
	}
	template<typename A>
	 Array<A>::Array(const SV::ArrayView<A>& elements)
	{
		if (elements.GetCount() == 0)
		{
			Allocate(DEFAULT_ARRAY_CAPACITY);
			return;
		}
		Allocate(elements.GetCount());
		AddRange(elements);
	}
	template<typename A>
	 Array<A>::Array(A* data, bool copy)
	{
		size_t dataCount = _msize(data);

		SV_ASSERT_T(dataCount == 0, SV::Array<A>::Array, "Could not ascertain array size when constructing array list");

		dataCount = dataCount / sizeof(A);

		if (copy)
		{
			Allocate(dataCount);
			for (size_t i = 0; i < dataCount; i++)
			{
				::new((void*)(this->internalArray + i)) A(data[i]);
			}
		}
		else
		{
			this->internalArray = data;
			this->capacity = dataCount;
		}
		this->count = dataCount;
	}
	template<typename A>
	 Array<A>::Array(A* data, size_t dataCount, bool copy)
	{
		if (copy)
		{
			Allocate(dataCount);
			for (size_t i = 0; i < dataCount; i++)
			{
				new(reinterpret_cast<void*>(this->internalArray + i)) A(data[i]);
			}
		}
		else
		{
			this->internalArray = data;
			this->capacity = dataCount;
		}
		this->count = dataCount;
	}
	template<typename A>
	 Array<A>::Array(const Array<A>& arr)
	{
		this->operator=(std::forward(arr));
	}
	template<typename A>
	 Array<A>::Array(Array<A>&& arr) noexcept
	{
		this->internalArray = arr.internalArray;
		this->count = arr.count;
		this->capacity = arr.capacity;
		this->typeCachePtr = arr.typeCachePtr;

		arr.internalArray = nullptr;
		arr.count = 0;
		arr.capacity = 0;
		arr.typeCachePtr = nullptr;
	}
	template<typename A>
	 Array<A>::~Array() noexcept
	{
		if constexpr (!std::is_trivially_destructible_v<A>)
		{
			for (size_t i = 0; i < this->count; i++)
			{
				this->internalArray[i].~A();
			}
		}

#ifdef _WIN32
		HeapFree(GetProcessHeap(), NULL, (void*)this->internalArray);
#else
		free((void*)this->internalArray);
#endif // _WIN32
		this->internalArray = nullptr;
		this->count = 0;
		this->capacity = 0;
	}

	//Assignment Operators
	template<typename A>
	 void Array<A>::operator=(const Array<A>& arr) noexcept
	{
		size_t i = 0;
		if constexpr (!std::is_trivially_destructible_v<A>)
		{
			for (i = 0; i < this->count; i++)
			{
				this->internalArray[i].~A();
			}
		}

		if (this->capacity < arr.count)  Allocate(arr.count);
		this->count = arr.count;
		this->typeCachePtr = arr.typeCachePtr;

		for (i = 0; i < this->count; i++)
		{
			if constexpr (std::is_move_constructible_v<A>) ::new(static_cast<void*>(this->internalArray + i)) A((A&&)arr.internalArray[i]);
			else ::new(static_cast<void*>(this->internalArray + i)) A((const A&)arr.internalArray[i]);
		}
	}
	template<typename A>
	 void Array<A>::operator=(Array<A>&& arr) noexcept
	{
		if (this != &arr)
		{
			this->~Array();

			this->internalArray = arr.internalArray;
			this->count = arr.count;
			this->capacity = arr.capacity;
			this->typeCachePtr = arr.typeCachePtr;

			arr.internalArray = nullptr;
			arr.count = 0;
			arr.capacity = 0;
		}
	}

	//Add and Emplace Functions
	template<typename A>
	 size_t Array<A>::Add()
	{
		static_assert(std::is_default_constructible_v<A>, "To add an element to the list without constructor arguments, a default constructor must be provided.");

		if (this->count + 1 > capacity)
		{
			Expand(1);
		}

		unsigned char* memoryLoc = reinterpret_cast<unsigned char*>(this->internalArray) + this->count * sizeof(A);
		new(memoryLoc) A();

		this->count++;
		return this->count - 1;
	}
	template<typename A>
	 size_t Array<A>::Add(const A& obj)
	{
		static_assert(std::is_copy_constructible_v<A>, "To add an element to the list by copy constructor, the object must have a valid copy constructor.");

		//Make sure obj is not in this array, as it may be invalidated
		if (this->count + 1 > capacity)
		{
			Expand(1);
		}

		A* memoryLoc = this->internalArray + this->count;
		new(memoryLoc) A((const A&)obj);

		this->count++;
		return this->count - 1;
	}
	template<typename A>
	 size_t Array<A>::Add(A&& obj)
	{
		static_assert(std::is_move_constructible_v<A>, "To add an element to the list by move constructor, the object must have a valid move constructor.");
		//Make sure obj is not in this array, as it may be invalidated
		if (this->count + 1 > capacity)
		{
			Expand(1);
		}

		A* memoryLoc = this->internalArray + this->count;
		::new(memoryLoc) A((A&&)(obj));

		this->count++;
		return this->count - 1;
	}
	template<typename A>
	size_t Array<A>::AddRange(const std::initializer_list<A>& objects)
	{
		return AddRange(ArrayView<A>((A*)objects.begin(), 0, objects.size()));
	}
	template<typename A>
	 size_t Array<A>::AddRange(const SV::ArrayView<A>& objects)
	{
		static_assert(std::is_copy_constructible_v<A>, "To add an element to the list by copy constructor, the object must have a valid copy constructor.");
		if (this->count + objects.GetCount() > capacity)
		{
			Expand(objects.GetCount());
		}

		A* memLoc = this->internalArray + this->count;

		for (size_t i = 0; i < objects.GetCount(); i++)
		{
			new(memLoc + i) A((const A&)objects[i]);
		}

		this->count += objects.GetCount();

		return this->count - 1;
	}
	template<typename A>
	size_t Array<A>::AddRangeMove(const std::initializer_list<A>& objects)
	{
		return AddRangeMove(ArrayView<A>((A*)objects.begin(), 0, objects.size()));
	}
	template<typename A>
	 size_t Array<A>::AddRangeMove(const SV::ArrayView<A>& objects)
	{
		static_assert(std::is_move_constructible_v<A>, "To add an element to the list by move constructor, the object must have a valid move constructor.");

		if (this->count + objects.GetCount() > capacity)
		{
			Expand(objects.GetCount());
		}

		A* memLoc = this->internalArray + this->count;

		for (size_t i = 0; i < objects.GetCount(); i++)
		{
			new(memLoc + i) A((A&&)(objects[i]));
		}

		this->count += objects.GetCount();

		return this->count - 1;
	}
	template<typename A>
	template<class... Args>
	 size_t Array<A>::Emplace(Args&& ... arguments)
	{
		//Make sure arguments is not in this array, as it may be invalidated
		if (this->count + 1 > capacity)
		{
			Expand(1);
		}

		A* memoryLoc = this->internalArray + this->count;
		new(memoryLoc) A(std::forward<Args>(arguments)...);

		this->count++;
		return this->count - 1;
	}
	template<typename A>
	 void* Array<A>::EmplacePtr()
	{
		if (this->count + 1 > capacity)
		{
			Expand(1);
		}

		unsigned char* memoryLoc = reinterpret_cast<unsigned char*>(this->internalArray) + this->count * sizeof(A);

		this->count++;
		return reinterpret_cast<void*>(memoryLoc);
	}

	//Remove Functions
	template<typename A>
	 void Array<A>::Remove(const A& obj)
	{
		const size_t index = SV::ArrayBase<A>::Find(obj);
		SV_ASSERT_T(index == IndexUndefined, SV::Array<A>::Remove, "Cannot find object to remove");
		Remove(index);
	}
	template<typename A>
	 void Array<A>::Remove(size_t index)
	{
		SV_ASSERT_T(index + 1 > this->count, SV::Array<A>::Remove(), "Index is greater than array this->count");

		this->internalArray[index].~A();

		unsigned char* indexPtr = reinterpret_cast<unsigned char*>(this->internalArray + index);
		unsigned char* endPtr = reinterpret_cast<unsigned char*>(this->internalArray + this->count - 1);

		for (; indexPtr < endPtr; indexPtr++)
		{
			(*indexPtr) = *(indexPtr + sizeof(A));
		}

		this->count--;

		if (this->count < capacity - retractionIncrement)
		{
			Retract(retractionIncrement);
		}
	}
	template<typename A>
	 void Array<A>::RemoveRange(size_t startIndex, size_t endIndex)
	{
		SV_ASSERT_T(startIndex + 1 > this->count, SV::Array<A>::RemoveRange(), "Start index is greater than array this->count");
		SV_ASSERT_T(endIndex + 1 > this->count, SV::Array<A>::RemoveRange(), "End index is greater than array this->count");
		SV_ASSERT_T(startIndex > endIndex, SV::Array<A>::RemoveRange(), "Start index cannot be greater than end index");
		size_t i = 0;

		for (i = startIndex; i < endIndex; i++)
		{
			this->internalArray[i].~A();
		}

		i = endIndex - startIndex;

		unsigned char* startBlockPtr = reinterpret_cast<unsigned char*>(this->internalArray + startIndex);
		unsigned char* endBlockPtr = reinterpret_cast<unsigned char*>(this->internalArray + endIndex);
		unsigned char* endOfArrayPtr = reinterpret_cast<unsigned char*>(this->internalArray + this->count - 1);

		A* ptr = (A*)endOfArrayPtr;

		for (; startBlockPtr < endBlockPtr && endBlockPtr < endOfArrayPtr;)
		{
			(*startBlockPtr) = (*endBlockPtr);
			startBlockPtr++;
			endBlockPtr++;
		}

		this->count -= i;

		if (this->count + 1 < capacity - i)
		{
			Retract(i);
		}
	}

	//Insert Functions
	template<typename A>
	 size_t Array<A>::Insert(size_t index, const A& obj)
	{
		static_assert(std::is_copy_constructible_v<A>, "To insert an element to the list by copy constructor, the object must have a valid copy constructor.");

		SV_ASSERT_T(index + 1 > this->count, SV::Array<A>::Insert, "Index cannot be greater than this->count");

		if (this->count + 1 > capacity)
		{
			Expand(1);
		}

		unsigned char* indexPtr = reinterpret_cast<unsigned char*>(this->internalArray + index);
		unsigned char* endPtr = reinterpret_cast<unsigned char*>(this->internalArray + this->count) + sizeof(A) - 1;

		for (; endPtr > indexPtr; endPtr--)
		{
			(*endPtr) = *(endPtr - sizeof(A));
		}

		this->count++;
		new(&this->internalArray[index]) A(obj);

		return index;
	}
	template<typename A>
	template <class... Args>
	 size_t Array<A>::InsertEmplace(size_t index, const Args&& ... arguments)
	{
		SV_ASSERT_T(index + 1 > this->count, SV::Array<A>::Insert, "Index cannot be greater than this->count");
		if (this->count + 1 > capacity)
		{
			Expand(1);
		}

		unsigned char* indexPtr = reinterpret_cast<unsigned char*>(this->internalArray + index);
		unsigned char* endPtr = reinterpret_cast<unsigned char*>(this->internalArray + this->count) + sizeof(A) - 1;

		for (; endPtr > indexPtr; endPtr--)
		{
			(*endPtr) = *(endPtr - sizeof(A));
		}

		this->count++;
		new(&this->internalArray[index]) A(arguments...);

		return index;
	}
	template<typename A>
	 void Array<A>::InsertRange(size_t index, const SV::ArrayView<A>& objects)
	{
		static_assert(std::is_copy_constructible_v<A>, "To insert an element to the list by copy constructor, the object must have a valid copy constructor.");

		SV_ASSERT_T(index + 1 > this->count, SV::Array<A>::InsertRange, "Index cannot be greater than this->count");
		if (this->count + objects.GetCount() + 1 > capacity)
		{
			Expand(objects.GetCount());
		}

		this->count += objects.GetCount();

		unsigned char* endBlockPtr = reinterpret_cast<unsigned char*>(this->internalArray + index + objects.GetCount() - 1);
		unsigned char* endOfArrayPtr = reinterpret_cast<unsigned char*>(this->internalArray + this->count - 1) + sizeof(A) - 1;

		for (; endOfArrayPtr > endBlockPtr;)
		{
			(*endOfArrayPtr) = *(endOfArrayPtr - objects.GetCount() * sizeof(A));
			endOfArrayPtr--;
		}

		for (size_t i = 0; i < objects.GetCount(); i++)
		{
			new(&this->internalArray[i + index]) A(objects[i]);
		}
	}

	//
	template<typename A>
	 void Array<A>::Set(size_t index, const A& obj)
	{
		SV_ASSERT_T(index + 1 > this->count, SV::Array<A>::Set(), "Index cannot be greater than array this->count");

		static_assert(std::is_copy_assignable_v<A>, "To set an element, a valid copy assign operator= must be provided");

		this->internalArray[index] = obj;
	}
	template<typename A>
	void Array<A>::SetRange(size_t start, const std::initializer_list<A>& objs)
	{
		return SetRange(start, ArrayView<A>((A*)objs.begin(), 0, objs.size()));
	}
	template<typename A>
	 void Array<A>::SetRange(size_t start, const SV::ArrayView<A>& objs)
	{
		static_assert(std::is_copy_assignable_v<A>, "To set elements, a valid copy assign operator= must be provided");

		SV_ASSERT_T(start + 1 > this->count, SV::Array<A>::GetRange(), "Start index cannot be greater than the array this->count + 1");
		if (start + objs.GetCount() > capacity)
		{
			size_t amount = (start + objs.GetCount()) - this->capacity;
			Expand(amount);

			this->count += amount;
		}

		for (size_t i = 0; i < objs.GetCount(); i++)
		{
			this->internalArray[i + start] = objs.GetInternalArray()[i];
		}
	}
	template<typename A>
	 void Array<A>::SetByteCopy(size_t index, const A& obj)
	{
		SV_ASSERT_T(index + 1 > this->count, SV::Array<A>::SetByteCopy(), "Index is greater than array this->count");

		memcpy(&this->internalArray[index], &obj, sizeof(A));
	}

	//Trim and Clear Functions
	template<typename A>
	 void Array<A>::Trim()
	{
		if (this->capacity == this->count) return;
		this->Allocate(this->count);
	}
	template<typename A>
	 void Array<A>::Clear()
	{
		this->~Array();
	}
	template<typename A>
	 void Array<A>::ResizeToCapacity()
	{
		Retract(capacity - this->count);
	}

	template<typename A>
	void Array<A>::Empty()
	{
		for (unsigned int i = 0; i < this->count; i++)
		{
			this->internalArray[i].~A();
		}

		this->count = 0;
	}

	//Reserve and Resize Functions
	template<typename A>
	 void Array<A>::Reserve(size_t amount)
	{
		Expand(amount);
	}
	template<typename A>
	 void Array<A>::Resize(size_t numberOfElements)
	{
		if (numberOfElements > capacity - this->count)
		{
			Expand(numberOfElements - (capacity - this->count) + 1);
		}

		if constexpr (std::is_default_constructible_v<A>)
		{
			for (size_t i = this->count; i < numberOfElements; i++)
			{
				::new((void*)(this->internalArray + i)) A();
			}
		}

		this->count += numberOfElements;
	}

	//Retract and Expand Functions
	template<typename A>
	 void Array<A>::Retract(size_t amount)
	{
		long result = long(capacity) - long(amount);
		Allocate(result < 0 ? 0 : (size_t)result);
	}
	template<typename A>
	 void Array<A>::Expand(size_t amount)
	{
		amount += capacity;
		size_t geometricFactor = capacity + capacity / 2;
		this->Allocate(geometricFactor < amount ? amount : geometricFactor);
	}

	//Memory Management Functions
#ifdef _WIN32
	template<typename A>
	void Array<A>::Allocate(size_t newCapacity)
	{
		HANDLE myHeap = GetProcessHeap();

		if (this->internalArray == nullptr)
		{
			this->internalArray = (A*)HeapAlloc(myHeap, HEAP_ZERO_MEMORY, newCapacity * sizeof(A));
			if (this->internalArray == nullptr)
			{
				SV_FATAL_T(SV::Array<A>::Allocate, "Memory from realloc could not be allocated");
				SV_THROW_RELEASE(std::overflow_error("Memory from realloc could not be allocated"));
			}
			capacity = newCapacity;
			return;
		}


		A* tempArray = (A*)HeapReAlloc(myHeap, HEAP_REALLOC_IN_PLACE_ONLY | HEAP_ZERO_MEMORY, (void*)this->internalArray, newCapacity * sizeof(A));

		if (newCapacity < this->count)
		{
			if constexpr (!std::is_trivially_destructible_v<A>)
			{
				for (size_t i = this->count; i > newCapacity; i--)
				{
					this->internalArray[i].~A();
				}
			}
			this->count = newCapacity;
		}

		if (tempArray == nullptr)
		{
			tempArray = (A*)HeapAlloc(myHeap, HEAP_ZERO_MEMORY, newCapacity * sizeof(A));
			if (tempArray == nullptr)
			{
				SV_FATAL_T(SV::Array<A>::Allocate, "Memory from realloc could not be allocated");
				SV_THROW_RELEASE(std::overflow_error("Memory from realloc could not be allocated"));
			}

			for (size_t i = 0; i < this->count; i++)
			{
				if constexpr (std::is_trivially_move_constructible_v<A>)
				{
					::new((void*)(tempArray + i)) A(static_cast<A&&>(this->internalArray[i]));
				}
				else
				{
					::new((void*)(tempArray + i)) A(static_cast<const A&>(this->internalArray[i]));
					if constexpr (!std::is_trivially_destructible_v<A>) this->internalArray[i].~A();
				}
			}

			HeapFree(myHeap, NULL, (void*)this->internalArray);
		}

		this->internalArray = tempArray;
		capacity = newCapacity;
	}
#else
	template<typename A>
	void Array<A>::Allocate(size_t newCapacity)
	{
		A* tempArray = (A*)malloc(newCapacity * sizeof(A));
		if (tempArray == nullptr)
		{
			SV_FATAL_T(SV::Array<A>::Allocate, "Memory from realloc could not be allocated");
			SV_THROW_RELEASE(std::overflow_error("Memory from realloc could not be allocated"));
		}

		if (this->internalArray == nullptr)
		{
			this->internalArray = tempArray;
			capacity = newCapacity;
			return;
		}

		if (newCapacity < this->count)
		{
			if constexpr (!std::is_trivially_destructible_v<A>)
			{
				for (size_t i = this->count; i > newCapacity; i--)
				{
					this->internalArray[i].~A();
				}
			}
			this->count = newCapacity;
		}

		for (size_t i = 0; i < this->count; i++)
		{
			if constexpr (std::is_trivially_move_constructible_v<A>)
			{
				::new((void*)(tempArray + i)) A(static_cast<A&&>(this->internalArray[i]));
			}
			else
			{
				::new((void*)(tempArray + i)) A(static_cast<const A&>(this->internalArray[i]));
				if constexpr (!std::is_trivially_destructible_v<A>) this->internalArray[i].~A();
			}
		}

		free((void*)this->internalArray);
		this->internalArray = tempArray;
		capacity = newCapacity;
	}
#endif

#pragma endregion

#pragma region Array Span Implementations
	//
	template<typename A>
	 ArraySpan<A>::ArraySpan(A* array, size_t offset, size_t length)
	{
		this->Reassign(array, offset, length);
	}
	template<typename A>
	 ArraySpan<A>::ArraySpan(ArrayBase<A>& array, size_t offset, size_t length)
	{
		if (length == 0) length = array.GetCount();
		SV_ASSERT_T(offset + 1 > length, ArraySpan<A>::ArraySpan, "The offset parameter provided is not in the range of the array.");

		this->internalArray = array.GetInternalArray() + offset;
		this->count = length;
	}
	template<typename A>
	 ArraySpan<A>::ArraySpan(std::vector<A>& vec, size_t offset, size_t length)
	{
		if (length == 0) length = vec.size();
		SV_ASSERT_T(offset + 1 > length, ArraySpan<A>::ArraySpan, "The offset parameter provided is not in the range of the array.");

		this->internalArray = vec.begin() + offset;
		this->count = length;
	}
	template<typename A>
	ArraySpan<A>::ArraySpan(const ArraySpan<A>& arraySpan)
	{
		this->internalArray = arraySpan.internalArray;
		this->count = arraySpan.count;
		this->typeCachePtr = arraySpan.typeCachePtr;
	}
	template<typename A>
	ArraySpan<A>::ArraySpan(ArraySpan<A>&& arraySpan) noexcept
	{
		this->internalArray = arraySpan.internalArray;
		this->count = arraySpan.count;
		this->typeCachePtr = arraySpan.typeCachePtr;

		arraySpan.internalArray = nullptr;
		arraySpan.count = 0;
		arraySpan.typeCachePtr.reset();
	}
	template<typename A>
	template<size_t size>
	 ArraySpan<A>::ArraySpan(std::array<A, size>& arrayIn, size_t offset, size_t length)
	{
		if (length == 0) length = size;

		SV_ASSERT_T(offset + 1 > length, ArrayView<A>::ArrayView, "The offset parameter provided is not in the range of the array.");

		this->internalArray = const_cast<A*>(arrayIn.begin() + offset);
		this->count = length;
	}
	template<typename A>
	 ArraySpan<A>& ArraySpan<A>::operator=(const ArraySpan<A>& obj)
	{
		if (this != &obj)
		{
			this->internalArray = obj.internalArray;
			this->count = obj.count;
		}
		return *this;
	}
	template<typename A>
	 ArraySpan<A>& ArraySpan<A>::operator=(ArraySpan<A>&& obj) noexcept
	{
		if (this != &obj)
		{
			this->internalArray = obj.internalArray;
			this->count = obj.count;

			obj.internalArray = nullptr;
			obj.count = 0;
		}
		return *this;
	}
	template<typename A>
	ArraySpan<A>& ArraySpan<A>::operator=(const ArrayBase<A>& arr)
	{
		if ((ArrayBase<A>*)this != &arr)
		{
			this->internalArray = arr.internalArray;
			this->count = arr.count;
		}
		return *this;
	}
	template<typename A>
	ArraySpan<A>& ArraySpan<A>::operator=(const std::vector<A>& arr)
	{
		this->internalArray = arr.begin();
		this->count = arr.size();
		return *this;
	}
	template<typename A>
	template<unsigned int sizeIn>
	ArraySpan<A>& ArraySpan<A>::operator=(std::array<A, sizeIn>& arrayIn)
	{
		this->internalArray = arrayIn.begin();
		this->count = arrayIn.size();
	}
	//
	template<typename A>
	 void ArraySpan<A>::Reassign(ArrayBase<A>& array, size_t offset, size_t length)
	{
		if (length == 0) length = array.count;
		SV_ASSERT_T(offset + 1 > length, ArrayView<A>::ArrayView, "The offset parameter provided is not in the range of the array.");

		this->internalArray = array.internalArray + offset;
		this->count = length;
	}
	//
	template<typename A>
	 void ArraySpan<A>::Reassign(A* array, size_t offset, size_t length)
	{
		// if (length == 0 && array != nullptr)
		// {
		// 	length = _msize(const_cast<A*>(array)) / sizeof(A);
		// 	if (length == 0)
		// 	{
		// 		SV_FATAL_T(ArrayView<A>::ArrayView, "The size of the array pointer passed to ArrayView<A>::ArrayView cannot be discerned. Please ensure the pointer is to a heap-allocated valid memory region, or specify the size when constructing the array view.");
		// 		SV_THROW_RELEASE(std::runtime_error("The size of the array pointer passed to ArrayView<A>::ArrayView cannot be discerned. Please ensure the pointer is to a heap-allocated valid memory region, or specify the size when constructing the array view."));
		// 	}
		// }

		if (array == nullptr) offset = 0;

		SV_ASSERT_T(offset + 1 > length && length != 0, ArrayView<A>::ArrayView, "The offset parameter provided is not in the range of the array.");

		this->internalArray = array + offset;
		this->count = length;
	}
	//
#pragma endregion

#pragma region Array View Implementations
	//
	template<typename A>
	 ArrayView<A>::ArrayView()
	{
		internalArray = nullptr;
		arrayLength = 0;
	}
	//
	template<typename A>
	 ArrayView<A>::ArrayView(const A* array, size_t offset, size_t length)
	{
		Reassign(array, offset, length);
	}
	//
	template<typename A>
	 ArrayView<A>::ArrayView(const ArrayBase<A>& array, size_t offset, size_t length)
	{
		if (length == 0) length = array.GetCount();
		SV_ASSERT_T(offset + 1 > length, ArrayView<A>::ArrayView, "The offset parameter provided is not in the range of the array.");

		internalArray = const_cast<A*>(array.GetInternalArray() + offset);
		arrayLength = length;
	}
	//
	template<typename A>
	 ArrayView<A>::ArrayView(const std::vector<A>& vec, size_t offset, size_t length)
	{
		if (length == 0) length = vec.size();
		SV_ASSERT_T(offset + 1 > length, ArrayView<A>::ArrayView, "The offset parameter provided is not in the range of the array.");

		internalArray = const_cast<A*>(vec.begin() + offset);
		arrayLength = length;
	}
	//
	template<typename A>
	template<size_t sizeIn>
	 ArrayView<A>::ArrayView(const std::array<A, sizeIn>& arrayIn, size_t offset, size_t length)
	{
		if (length == 0) length = sizeIn;

		SV_ASSERT_T(offset + 1 > length, ArrayView<A>::ArrayView, "The offset parameter provided is not in the range of the array.");

		internalArray = const_cast<A*>(arrayIn.begin() + offset);
		arrayLength = length;
	}
	//
	template<typename A>
	 const A& ArrayView<A>::Get(size_t index) const
	{
#ifdef SV_ARRAY_DEBUG
		if (index + 1 > arrayLength)
		{
			SV_FATAL_T(ArrayView<A>::Get, "Index is out of range");
		}
		if (internalArray == nullptr)
		{
			SV_FATAL_T(ArrayView<A>::Get, "Internal array is nullptr.");
		}
#endif

		return internalArray[index];
	}
	//
	template<typename A>
	 void ArrayView<A>::Reassign(const A* array, size_t offset, size_t length)
	{
		// if (length == 0 && array != nullptr)
		// {
		// 	length = _msize(const_cast<A*>(array)) / sizeof(A);
		// 	if (length == 0)
		// 	{
		// 		SV_FATAL_T(ArrayView<A>::ArrayView, "The size of the array pointer passed to ArrayView<A>::ArrayView cannot be discerned. Please ensure the pointer is to a heap-allocated valid memory region, or specify the size when constructing the array view.");
		// 		throw std::runtime_error("The size of the array pointer passed to ArrayView<A>::ArrayView cannot be discerned. Please ensure the pointer is to a heap-allocated valid memory region, or specify the size when constructing the array view.");
		// 	}
		// }

		if (array == nullptr) offset = 0;

		SV_ASSERT_T(offset + 1 > length && length != 0, ArrayView<A>::ArrayView, "The offset parameter provided is not in the range of the array.");

		internalArray = const_cast<A*>(array + offset);
		arrayLength = length;
	}
	//
#pragma endregion

#pragma region Array Group Implementations

	//Array Allocation Functions
	template<typename A>
	 SV::ArraySpan<A>& ArrayGroup::AllocateArray(size_t elementCount, unsigned int* index) noexcept
	{
		if (currentByteIndex + elementCount * sizeof(A) > byteBuffer.GetCount())
		{
			byteBuffer.Resize(currentByteIndex + 1 + elementCount * sizeof(A));
		}

		size_t newObjIndex = arraySpans.Emplace(reinterpret_cast<void**>(&byteBuffer[currentByteIndex]), 0, elementCount);

		currentByteIndex += elementCount * sizeof(float);

		if (index != nullptr) (*index) = newObjIndex;

		return *reinterpret_cast<SV::ArraySpan<A>*>(&arraySpans[newObjIndex]);
	}

#pragma endregion

#pragma region Static Array Group Implementations

	//Constructors and Destructors
	template<unsigned int MaxSpans>
	 StaticArrayGroup<MaxSpans>::StaticArrayGroup(size_t byteSize) :
		byteBuffer(byteSize, true)
	{
	}

	//Array Allocation Functions
	template<unsigned int MaxSpans>
	template<typename A>
	 SV::ArraySpan<A>& StaticArrayGroup<MaxSpans>::AllocateArray(size_t elementCount, int index)
	{
		if (arraySpansOccupied == MaxSpans)
		{
			SV_FATAL_T(SV::StaticBufferedArray::AllocateArray, "No more array spans avalible to allocate another array.");
			SV_THROW_RELEASE(std::runtime_error("No more array spans avalible to allocate another array."));
		}
		if (currentByteIndex + elementCount * sizeof(A) > byteBuffer.GetCount())
		{
			byteBuffer.Resize(currentByteIndex + 1 + elementCount * sizeof(A));
		}

		if (index > -1)
		{
			SV_ASSERT_T(arraySpans[index] != nullptr, SV::StaticBufferedArray<>::AllocateArray, ("Array span with index" + std::to_string(index) + " is already occupied.").c_str());
		}
		else
		{
			index = (int)arraySpansOccupied;
		}

		arraySpans[index].Reassign(reinterpret_cast<unsigned char*>(&byteBuffer[currentByteIndex]), 0, elementCount);

		currentByteIndex += elementCount * sizeof(float);
		arraySpansOccupied++;

		return *reinterpret_cast<SV::ArraySpan<A>*>(&arraySpans[index]);
	}

#pragma endregion

}