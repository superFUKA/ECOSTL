#pragma once
#include "Memory.hpp"
#include "TypeInfo.hpp"

namespace utl
{
	template<class T>
	constexpr size_t AlignedSize()
	{
		constexpr size_t a = alignof(T);
		return (sizeof(T) + a - 1) / a * a;
	}



	template<typename Type>
	inline void AddTypeSize(size_t& result)
	{
		result += AlignedSize<Type>();
	}


	template<typename...Types>
	inline size_t TypesSize()
	{
		size_t result = 0;

		(AddTypeSize<Types>(result), ...);

		return result;
	}

	template<typename Type>
	inline void InitializeTypeMemories(char* begin, size_t& index, size_t num)
	{
		for (size_t i = 0; i < num; ++i)
		{
			void* voidPtr = static_cast<void*>(begin + index);
			Type* ptr = static_cast<Type*>(voidPtr);
			new (ptr) Type();
			index += AlignedSize<Type>();
		}
	}
	template<typename Type>
	inline void InitializeTypeMemories(char* begin, size_t& index, size_t num, const Type& src)
	{
		for (size_t i = 0; i < num; ++i)
		{
			void* voidPtr = static_cast<void*>(begin + index);
			Type* ptr = static_cast<Type*>(voidPtr);
			new (ptr) Type(src);
			index += AlignedSize<Type>();
		}
	}

	template<typename Type>
	inline void ReleaseTypeMemories(char* begin, size_t& index, size_t num)
	{
		for (size_t i = 0; i < num; ++i)
		{
			void* voidPtr = static_cast<void*>(begin + index);
			Type* ptr = static_cast<Type*>(voidPtr);
			ptr->~Type();
			index += AlignedSize<Type>();
		}
	}



	template<typename CheckType, typename SubType>
	inline void CheckEqualType(size_t num, size_t& index, size_t& typeIndex)
	{
		if (typeIndex == 0)
		{
			return;
		}
		if (utl::GetTypeID<CheckType>() == utl::GetTypeID<SubType>())
		{
			--typeIndex;
		}
		if (typeIndex == 0)
		{
			return;
		}
		index += AlignedSize<CheckType>() * num;
	}

	template<typename...Types>
	class DynamicMultiMemoryArray
	{
	public:
		DynamicMultiMemoryArray(const size_t size, MemoryAllocatorAPI* alloc = nullptr)
			: alloc_(alloc)
			, begin_(alloc_.Allocate(TypesSize<Types...>()* size))
			, size_(size)
		{
			//size_t index = 0;
			//(InitializeTypeMemories<Types>(static_cast<char*>(begin_), index, size), ...);
		}

		DynamicMultiMemoryArray(const size_t size, MemoryAllocatorAPI* alloc, Types&&...srces)
			: alloc_(alloc)
			, begin_(alloc_.Allocate(TypesSize<Types...>()* size))
			, size_(size)
		{
			//size_t index = 0;
			//(InitializeTypeMemories<Types>(static_cast<char*>(begin_), index, size, srces), ...);
		}


		~DynamicMultiMemoryArray()
		{
			//size_t index = 0;
			//(ReleaseTypeMemories<Types>(static_cast<char*>(begin_), index, size_), ...);
			//alloc_.Deallocate(begin_);
		}


		template<typename Type, size_t TYPE_INDEX = 1>
		Type* GetBegin()
		{
			size_t beginIndex = 0;
			// 最初に見つかったテンプレート引数と同じ型の先頭アドレスの位置を取得
			(CheckEqualType<Type, Types>(size_, beginIndex, TYPE_INDEX), ...);

			char* charPtr = static_cast<char*>(begin_);
			void* resultVoidPtr = static_cast<void*>(charPtr + beginIndex);
			Type* result = static_cast<Type*>(resultVoidPtr);

			return result;
		}

		template<typename Type>
		Type* Get(size_t index, size_t typeIndex = 1)
		{
			Type* begin = GetBegin<Type>(typeIndex);

			return begin + index;
		}




	private:

		MemoryAllocatorHolder alloc_;

		void* begin_;

		size_t size_;
	};
}
