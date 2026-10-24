#pragma once
#include "src/Memory/Memory.hpp"

namespace utl
{
	template<typename Type, size_t SIZE>
	class StaticArray
	{
	public:

		//--------------------------------------
		// 作成
		//--------------------------------------

		StaticArray()
			:mem_()
		{
			for (size_t i = 0; i < SIZE; ++i)
			{
				new (Get(i)) Type();
			}
		}

		template<typename...ArgTypes>
		StaticArray(ArgTypes&&...args)
			:mem_()
		{
			for (size_t i = 0; i < SIZE; ++i)
			{
				new (Get(i)) Type(args...);
			}
		}

		//--------------------------------------
		// 開放
		//--------------------------------------

		~StaticArray()
		{
			for (size_t i = 0; i < SIZE; ++i)
			{
				Get(i)->~Type();
			}
		}

		//--------------------------------------
		// 取得
		//--------------------------------------

		Type& operator[](const size_t index)const
		{
			assert(index < SIZE && "範囲外アクセスです");
			return *Get(index);
		}

		Type* Data()const
		{
			return Get(0);
		}

		constexpr size_t Size()const
		{
			return SIZE;
		}

		void* GetVoid(const size_t index)
		{
			assert(index < SIZE && "範囲外アクセスです");

			char* ptr = &mem_[index * sizeof(Type)];
			return static_cast<void*>(ptr);
		}

		Type* Get(const size_t index)
		{
			void* ptr = GetVoid(index);
			return static_cast<Type*>(ptr);
		}


	private:
		
		alignas(Type) char mem_[SIZE * sizeof(Type)];

	};
}