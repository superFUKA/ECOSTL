#pragma once
#include "src/Memory/Memory.hpp"
#include "src/TypeInfo/TypeInfo.hpp"

namespace utl
{
	template<class T>
	constexpr uint32_t AlignedSize()
	{
		constexpr uint32_t a = alignof(T);
		return (sizeof(T) + a - 1) / a * a;
	}



	template<typename Type>
	inline void AddTypeSize(uint32_t& result)
	{
		result += AlignedSize<Type>();
	}


	template<typename...Types>
	inline uint32_t TypesSize()
	{
		uint32_t result = 0;

		(AddTypeSize<Types>(result), ...);

		return result;
	}

	template<typename Type>
	inline void InitializeTypeMemories(char* begin, uint32_t& index, uint32_t num)
	{
		for (uint32_t i = 0; i < num; ++i)
		{
			void* voidPtr = static_cast<void*>(begin + index);
			Type* ptr = static_cast<Type*>(voidPtr);
			new (ptr) Type();
			index += AlignedSize<Type>();
		}
	}
	template<typename Type>
	inline void InitializeTypeMemories(char* begin, uint32_t& index, uint32_t num, const Type& src)
	{
		for (uint32_t i = 0; i < num; ++i)
		{
			void* voidPtr = static_cast<void*>(begin + index);
			Type* ptr = static_cast<Type*>(voidPtr);
			new (ptr) Type(src);
			index += AlignedSize<Type>();
		}
	}

	template<typename Type>
	inline void ReleaseTypeMemories(char* begin, uint32_t& index, uint32_t num)
	{
		for (uint32_t i = 0; i < num; ++i)
		{
			void* voidPtr = static_cast<void*>(begin + index);
			Type* ptr = static_cast<Type*>(voidPtr);
			ptr->~Type();
			index += AlignedSize<Type>();
		}
	}



	template<typename CheckType, typename SubType>
	inline void CheckEqualType(uint32_t num, uint32_t& index, uint32_t& typeIndex)
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
	class DynamicMultiArray
	{
	public:
		DynamicMultiArray(const uint32_t size, MemoryAllocatorAPI* alloc = nullptr)
			: alloc_(alloc)
			, begin_(alloc_.Allocate(TypesSize<Types...>()* size))
			, size_(size)
		{
			uint32_t index = 0;
			(InitializeTypeMemories<Types>(static_cast<char*>(begin_), index, size), ...);
		}

		DynamicMultiArray(const uint32_t size, MemoryAllocatorAPI* alloc, Types&&...srces)
			: alloc_(alloc)
			, begin_(alloc_.Allocate(TypesSize<Types...>()* size))
			, size_(size)
		{
			uint32_t index = 0;
			(InitializeTypeMemories<Types>(static_cast<char*>(begin_), index, size, srces), ...);
		}


		~DynamicMultiArray()
		{
			uint32_t index = 0;
			(ReleaseTypeMemories<Types>(static_cast<char*>(begin_), index, size_), ...);
			alloc_.Free(begin_);
		}


		template<typename Type>
		Type* GetBegin(uint32_t typeIndex = 1)
		{
			uint32_t beginIndex = 0;
			// 最初に見つかったテンプレート引数と同じ型の先頭アドレスの位置を取得
			(CheckEqualType<Type, Types>(size_, beginIndex, typeIndex), ...);

			char* charPtr = static_cast<char*>(begin_);
			void* resultVoidPtr = static_cast<void*>(charPtr + beginIndex);
			Type* result = static_cast<Type*>(resultVoidPtr);

			return result;
		}

		template<typename Type>
		Type* Get(uint32_t index, uint32_t typeIndex = 1)
		{
			Type* begin = GetBegin<Type>(typeIndex);

			return begin + index;
		}




	private:

		MemoryAllocatorAPIHolder alloc_;

		void* begin_;

		uint32_t size_;
	};
}
