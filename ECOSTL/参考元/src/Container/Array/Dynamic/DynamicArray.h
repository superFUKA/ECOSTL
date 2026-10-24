#pragma once
#include "src/Memory/Memory.hpp"

namespace utl
{
	/// <summary>
	/// 要素数やメモリ確保のタイミングなどを動的に指定して作成する固定長の配列
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename Type, size_t SIZE>
	class DynamicArray
	{
	public:

		//--------------------------------------
		// 作成
		//--------------------------------------

		template<typename...ArgTypes>
		DynamicArray(MemoryAllocatorAPI* alloc = nullptr, ArgTypes&&...args)
			: alloc_(alloc)
			, begin_(static_cast<Type*>(alloc_.Allocate(SIZE * sizeof(Type))))
		{
			for (size_t i = 0; i < SIZE; ++i)
			{
				Type* ptr = begin_ + i;
				new (ptr) Type(args...);
			}
		}

		//--------------------------------------
		// 開放
		//--------------------------------------

		~DynamicArray()
		{
			if (begin_ == nullptr)
			{
				return;
			}

			for (size_t i = 0; i < SIZE; ++i)
			{
				Get(i)->~Type();
			}

			alloc_.Free(begin_);
			begin_ = nullptr;
		}

		//--------------------------------------
		// 取得
		//--------------------------------------

		Type& operator[](const size_t index)const
		{
			return *Get(index);
		}

		Type* Data()const
		{
			return begin_;
		}

		constexpr size_t Size()const
		{
			return SIZE;
		}

		Type* Get(const size_t index)const
		{
			assert(index < SIZE && "範囲外アクセスです");

			return begin_ + index;
		}

		MemoryAllocatorAPIHolder& GetMemoryAllocator()const
		{
			return alloc_.Get();
		}

	private:

		MemoryAllocatorAPIHolder alloc_;

		Type* begin_;

	};
}