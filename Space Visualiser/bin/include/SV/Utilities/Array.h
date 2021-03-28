#pragma once
#include<malloc.h>
#include<initializer_list>
#include<algorithm>
#include<vector>
#include<array>
#include<xmemory>
#include<unordered_map>

#include"SVMemory.h"
#include"SV/Utilities/Errors.h"

#ifdef SV_DEBUG
#define SV_ARRAY_DEBUG
#endif // SV_DEBUG


#define newep(arrayList) new(arrayList.EmplacePtr())

//To do list:
//Add "Static Array" class
//Fix array spans and array view
//Make array groups type safe
//Consider enforcing noexcept safety on move operations
//Clear type cache on every change

namespace SV
{
	template<typename A>
	class ArrayBase;
	template<typename A, size_t myCount>
	class StaticArray;
	template<typename A>
	class Array;
	template<typename A>
	class ArraySpan;
	template<typename A>
	class ArrayView;
	class ArrayGroup;
	template<unsigned int MaxSpans>
	class StaticArrayGroup;

	//Find an element in a std::vector container
	template<class T>
	int FindEleVec(const T& element, const std::vector<T>& vecObj, size_t startPos = 0)
	{
		if (int(startPos) > int(vecObj.size()) - 1)
		{
			return -1;
		}

		auto iteratorA = std::find(vecObj.begin() + startPos, vecObj.end(), element);

		if (iteratorA == vecObj.end())
		{
			return -1;
		}
		else
		{
			return (int)std::distance(vecObj.begin(), iteratorA);
		}
	}
	//Find elements in a std::vector container
	template<class T>
	std::vector<int> FindEleAllVec(const T& element, const std::vector<T>& vecObj, size_t startPos = 0)
	{
		std::vector<int> indicies;

		int index = -1;

		do
		{
			index = FindEleVec<T>(element, vecObj, startPos);
			if (indicies.size() == 0 || index > -1) indicies.push_back(index);
			startPos = (size_t)index + 1;
		} while (index > -1);

		return static_cast<std::vector<int>&&>(indicies);
	}

	constexpr size_t DEFAULT_ARRAY_CAPACITY = 10;
	constexpr size_t DEFAULT_REALLOCATION_SIZE = 125;

	using ABTypeCache = std::unordered_map<size_t, size_t>;

	//
	template<typename A>
	class ArrayBase abstract
	{
	public:
		//
		ArrayBase() {}
		ArrayBase(const ArrayBase<A>&) = delete;
		ArrayBase(ArrayBase<A>&&) = delete;
		virtual ~ArrayBase() noexcept {}
		//
		void operator=(const ArrayBase<A>&) = delete;
		void operator=(ArrayBase<A>&&) = delete;
		bool operator==(const ArrayBase<A>& arr) const;
		bool operator!=(const ArrayBase<A>& arr) const;
		//
		[[nodiscard]] A& operator[] (const size_t index);
		[[nodiscard]] const A& operator[] (const size_t index) const;
		[[nodiscard]] const A& Get(size_t index) const;
		[[nodiscard]] A& Get(size_t index);
		[[nodiscard]] A* GetPtr(size_t index);
		[[nodiscard]] SV::Array<A> GetRange(size_t startIndex, size_t endIndex) const;
		//
		template<typename Type, bool useCache = true>
		[[nodiscard]] size_t GetInstanceIndex() const;
		template<typename Type, bool useCache = true>
		[[nodiscard]] const Type* GetInstance() const;
		template<typename Type, bool useCache = true>
		[[nodiscard]] Type* GetInstance();
		template<typename Type>
		[[nodiscard]] SV::Array<size_t> GetAllInstanceIndicies() const;
		template<typename Type>
		[[nodiscard]] void GetAllInstances(SV::Array<Type*>& instances) const;
		//
		const Ref<ABTypeCache> GetTypeCache() const { return typeCachePtr; }
		void ClearTypeCache() const;
		//
		[[nodiscard]] inline A& First()
		{
			return internalArray[0];
		}
		[[nodiscard]] inline const A& First() const
		{
			return internalArray[0];
		}
		[[nodiscard]] inline A& Last()
		{
			return internalArray[this->count - 1];
		}
		[[nodiscard]] inline const A& Last() const
		{
			return internalArray[this->count - 1];
		}
		inline const A* begin() const { return internalArray; }
		inline const A* end() const { return internalArray + this->count; }
		inline A* begin() { return internalArray; }
		inline A* end() { return internalArray + this->count; }
		//
		virtual void Set(size_t index, const A& obj);
		virtual void SetRange(size_t start, const std::initializer_list<A>& objs);
		virtual void SetRange(size_t start, const SV::ArrayView<A>& objs);
		virtual void SetByteCopy(size_t index, const A& obj);
		//
		void Move(size_t index, size_t newIndex);
		void Swap(size_t index1, size_t index2);
		//
		size_t Find(const A& obj, size_t offset = 0) const;
		[[nodiscard]] SV::Array<A> FindAll(const A& obj, size_t offset = 0) const;
		void FindAll(const A& obj, SV::Array<int>& items, size_t offset = 0) const;
		//
		inline const bool Exists(unsigned int index) const { return index < this->count; }
		inline const bool IsEmpty() const { return count == 0; }
		//
		inline size_t GetCount() const { return this->count; }
		inline const A* GetInternalArray() const { return internalArray; }
		inline A* GetInternalArray() { return internalArray; }
		//
	protected:
		//
		A* internalArray = nullptr;
		size_t count = 0;
		//
		mutable Ref<ABTypeCache> typeCachePtr = nullptr;
	};

	template<typename A, size_t myCount>
	class StaticArray : public ArrayBase<A>
	{
	public:
		StaticArray();
		StaticArray(const std::initializer_list<A>& dataIn);
		StaticArray(const ArrayBase<A>& arrayIn);
		StaticArray(const ArrayView<A>& arrayIn);
		StaticArray(const StaticArray<A, myCount>& arrayIn);
		StaticArray(StaticArray<A, myCount>&& arrayIn) noexcept;
		virtual ~StaticArray() noexcept;

		StaticArray<A, myCount>& operator=(const StaticArray<A, myCount>& arrayIn);
		StaticArray<A, myCount>& operator=(StaticArray<A, myCount>&& arrayIn) noexcept;

	private:
		unsigned char staticArray[myCount * sizeof(A)];
	};


	//
	template<typename A>
	class Array : public ArrayBase<A>
	{
	public:
		//
		Array();
		explicit Array(size_t capacity, bool resize = true);
		Array(const std::initializer_list<A>& elements);
		explicit Array(const SV::ArrayView<A>& elements);
		explicit Array(A* data, bool copy = false);
		explicit Array(A* data, size_t dataCount, bool copy = false);
		Array(const Array<A>& arr);
		Array(Array<A>&& arr) noexcept;
		virtual ~Array() noexcept;
		//
		void operator=(const Array<A>& arr) noexcept;
		void operator=(Array<A>&& arr) noexcept;
		//
		size_t Add();
		size_t Add(const A& obj);
		size_t Add(A&& obj);
		size_t AddRange(const std::initializer_list<A>& objects);
		size_t AddRange(const SV::ArrayView<A>& objects);
		size_t AddRangeMove(const std::initializer_list<A>& objects);
		size_t AddRangeMove(const SV::ArrayView<A>& objects);
		template <class... Args>
		size_t Emplace(Args&& ... arguments);
		void* EmplacePtr();
		//
		void Remove(const A& obj);
		void Remove(size_t index);
		void RemoveRange(size_t startIndex, size_t endIndex);
		//
		size_t Insert(size_t index, const A& obj);
		template <class... Args>
		size_t InsertEmplace(size_t index, const Args&& ... arguments);
		void InsertRange(size_t index, const SV::ArrayView<A>& objects);
		//
		virtual void Set(size_t index, const A& obj) override;
		virtual void SetRange(size_t start, const std::initializer_list<A>& objs) override;
		virtual void SetRange(size_t start, const SV::ArrayView<A>& objs) override;
		virtual void SetByteCopy(size_t index, const A& obj) override;
		//
		inline const size_t GetCapacity() const noexcept { return capacity; }
		//
		void Trim();
		void Clear();
		void ResizeToCapacity();
		//
		void Empty();
		//
		void Reserve(size_t amount);
		void Resize(size_t numberOfElements);
		//
		void Retract(size_t amount);
		void Expand(size_t amount);
		//
	private:
		friend class ArraySpan<A>;
		//
		void Allocate(size_t newCapacity);
		//
		size_t capacity = 0;
		const unsigned short retractionIncrement = (const unsigned short)200 / sizeof(A);
		//
	};

	//
	template<typename A>
	class ArraySpan : public ArrayBase<A>
	{
	public:
		ArraySpan() {}
		explicit ArraySpan(A* array, size_t offset = 0, size_t length = 0);
		ArraySpan(ArrayBase<A>& array, size_t offset = 0, size_t length = 0);
		ArraySpan(std::vector<A>& vec, size_t offset = 0, size_t length = 0);
		template<size_t size>
		ArraySpan(std::array<A, size>& arrayIn, size_t offset = 0, size_t length = 0);
		ArraySpan(const ArraySpan<A>& arraySpan);
		ArraySpan(ArraySpan<A>&& arraySpan) noexcept;
		//
		inline const bool operator==(const A* const ptr) { return this->internalArray == ptr; }
		inline const bool operator!=(const A* const ptr) { return this->internalArray != ptr; }
		//
		ArraySpan<A>& operator=(const ArraySpan<A>& obj);
		ArraySpan<A>& operator=(ArraySpan<A>&& obj) noexcept;
		ArraySpan<A>& operator=(const ArrayBase<A>& arr);
		ArraySpan<A>& operator=(const std::vector<A>& arr);
		template<unsigned int sizeIn>
		ArraySpan<A>& operator=(std::array<A, sizeIn>& arrayIn);
		//
		inline void Reassign(ArrayBase<A>& array, size_t offset = 0, size_t length = 0);
		inline void Reassign(A* array, size_t offset = 0, size_t length = 0);
		//
		const bool IsEqual(const ArraySpan<A>& obj) { return this->internalArray == obj.internalArray && this->count == obj.count; }
		const bool IsNotEqual(const ArraySpan<A>& obj) { return this->internalArray != obj.internalArray || this->count != obj.count; }
		//
	private:
		friend class Array<A>;
	};

	//
	template<typename A>
	class ArrayView
	{
	public:
		//
		ArrayView();
		explicit ArrayView(const A* array, size_t offset = 0, size_t length = 0);
		ArrayView(const ArrayBase<A>& array, size_t offset = 0, size_t length = 0);
		ArrayView(const std::vector<A>& vec, size_t offset = 0, size_t length = 0);
		template<size_t sizeIn>
		explicit ArrayView(const std::array<A, sizeIn>& arrayIn, size_t offset = 0, size_t length = 0);
		ArrayView(const ArrayView<A>& arrayView)
		{
			internalArray = arrayView.internalArray;
			arrayLength = arrayView.arrayLength;
		}
		ArrayView(ArrayView<A>&& arrayView)
		{
			internalArray = arrayView.internalArray;
			arrayLength = arrayView.arrayLength;

			arrayView.internalArray = nullptr;
			arrayView.arrayLength = 0;
		}
		//
		inline [[nodiscard]] const A& Get(size_t index) const;
		inline [[nodiscard]] const A& operator[](size_t index) const { return Get(index); }
		//
		inline const A* GetInternalArray() const { return internalArray; }
		inline const size_t GetCount() const { return arrayLength; }
		//
		void Reassign(const A* array, size_t offset = 0, size_t length = 0);
		//
		[[nodiscard]] inline const A& First() const
		{
			return internalArray[0];
		}
		[[nodiscard]] inline const A& Last() const
		{
			return internalArray[this->count - 1];
		}
		inline const A* begin() const { return internalArray; }
		inline const A* end() const { return internalArray + arrayLength; }
		//
#pragma region Boolean Operators
		//
		const bool operator==(const SV::ArrayView<A>& other) const
		{
			return internalArray == other.internalArray && arrayLength == other.arrayLength;
		}
		const bool operator==(const A* const ptr) const
		{
			return internalArray == ptr;
		}
		const bool operator!=(const SV::ArrayView<A>& other) const
		{
			return internalArray != other.internalArray || arrayLength != other.arrayLength;
		}
		const bool operator!=(const A* const ptr) const
		{
			return internalArray != ptr;
		}
		//
#pragma endregion
		//
#pragma region Assignment Operators
		//
		ArrayView<A>& operator=(const ArrayView<A>& arrayView)
		{
			internalArray = arrayView.internalArray;
			arrayLength = arrayView.arrayLength;
			return *this;
		}
		ArrayView<A>& operator=(ArrayView<A>&& arrayView)
		{
			if (&arrayView != this)
			{
				internalArray = arrayView.internalArray;
				arrayLength = arrayView.arrayLength;

				arrayView.internalArray = nullptr;
				arrayView.arrayLength = 0;
			}
			return*this;
		}
		ArrayView<A>& operator=(const ArrayBase<A>& arrIn)
		{
			internalArray = arrIn.GetInternalArray();
			arrayLength = arrIn.GetCount();
			return*this;
		}
		ArrayView<A>& operator=(const std::vector<A>& arrIn)
		{
			internalArray = arrIn.begin();
			arrayLength = arrIn.size();
			return*this;
		}
		template<size_t sizeIn>
		ArrayView<A>& operator=(const std::array<A, sizeIn>& arrIn)
		{
			internalArray = arrIn.begin();
			arrayLength = sizeIn;
			return*this;
		}
		//
#pragma endregion
		//
	protected:
		//
		const A* internalArray = nullptr;
		size_t arrayLength = 0;
		//
	};

	//
	class ArrayGroup
	{
	public:
		ArrayGroup(size_t byteSize = DEFAULT_ARRAY_CAPACITY);
		//
		template<typename A>
		SV::ArraySpan<A>& AllocateArray(size_t elementCount, unsigned int* index = nullptr) noexcept;
		template<typename A>
		inline SV::ArraySpan<A>& GetArray(unsigned int index) noexcept
		{
			return *reinterpret_cast<SV::ArrayView<A>*>(&arraySpans[index]);
		}
		template<typename A>
		inline const SV::ArraySpan<A>& GetArray(unsigned int index) const noexcept
		{
			return *reinterpret_cast<const SV::ArrayView<A>*>(&arraySpans[index]);
		}
		//
		inline const SV::Array<SV::ArraySpan<unsigned char>>& GetArraySpans() const noexcept { return arraySpans; }
		inline const size_t GetNoOfSpans() const noexcept { return arraySpans.GetCount(); }
		//
	private:
		SV::Array<SV::ArraySpan<unsigned char>> arraySpans;
		SV::Array<unsigned char> byteBuffer;
		size_t currentByteIndex = 0;
	};

	//
	template<unsigned int MaxSpans>
	class StaticArrayGroup
	{
	public:
		StaticArrayGroup(size_t byteSize = DEFAULT_ARRAY_CAPACITY);
		//
		template<typename A>
		SV::ArraySpan<A>& AllocateArray(size_t elementCount, int index = -1);
		template<typename A>
		inline SV::ArraySpan<A>& GetArray(unsigned int index) noexcept
		{
			SV_ASSERT_T(index + 1 > MaxSpans, SV::StaticBufferedArray::GetArray, "Index is out of range");
			return *reinterpret_cast<SV::ArraySpan<A>*>(&arraySpans[index]);
		}
		template<typename A>
		inline const SV::ArraySpan<A>& GetArray(unsigned int index) const noexcept
		{
			SV_ASSERT_T(index + 1 > MaxSpans, SV::StaticBufferedArray::GetArray, "Index is out of range");
			return *reinterpret_cast<const SV::ArraySpan<A>*>(&arraySpans[index]);
		}
		//
		inline const SV::ArraySpan<unsigned char>* GetArraySpans() const noexcept { return arraySpans; }
		inline unsigned int GetNoOfSpanCapacity() const noexcept { return MaxSpans; }
		inline unsigned int GetNoOfSpans() const noexcept { return arraySpansOccupied; }
		//
	private:
		SV::ArraySpan<unsigned char> arraySpans[MaxSpans] = {};
		SV::Array<unsigned char> byteBuffer;
		size_t currentByteIndex = 0;
		unsigned int arraySpansOccupied = 0;
	};



}

#include"Array.cpp"
