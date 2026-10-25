#pragma once
#include <cassert>
#include <cstdint>
#include <new>
#include "../../Core/Utility.h"
#include "../../Type/TypeTraits.h"

namespace utl
{
	/// <summary>
	/// 要素数をコンパイル時に決める固定長の配列
	/// 要素のメモリは配列自身が持ち、メモリの確保は行わない
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <typeparam name="SIZE">1以上</typeparam>
	template<typename Type, uint32_t SIZE>
	class StaticArray
	{
		static_assert(SIZE > 0, "要素数は1以上にしてください");

	public:
		// 作成
		//----------------------

		/// <summary>
		/// すべての要素をデフォルトコンストラクタで作成する
		/// </summary>
		StaticArray()
		{
			for (uint32_t i = 0; i < SIZE; ++i)
			{
				new (GetVoid(i)) Type();
			}
		}

		/// <summary>
		/// すべての要素を同じコンストラクタ引数で作成する
		/// </summary>
		template<typename FirstArg, typename...ArgTypes,
			EnableIfType<!IsSameV<RemoveCVType<RemoveReferenceType<FirstArg>>, StaticArray>, int> = 0>
		explicit StaticArray(FirstArg&& first, ArgTypes&&...args)
		{
			for (uint32_t i = 0; i < SIZE; ++i)
			{
				new (GetVoid(i)) Type(first, args...);
			}
		}

		// 代入
		//----------------------

		StaticArray(const StaticArray& src)
		{
			if constexpr (IsTriviallyCopyableV<Type>)
			{
				MemCopy(mem_, src.mem_, sizeof(mem_));
			}
			else
			{
				for (uint32_t i = 0; i < SIZE; ++i)
				{
					new (GetVoid(i)) Type(src[i]);
				}
			}
		}
		StaticArray& operator=(const StaticArray& src)
		{
			if (this != &src)
			{
				for (uint32_t i = 0; i < SIZE; ++i)
				{
					(*this)[i] = src[i];
				}
			}
			return *this;
		}

		StaticArray(StaticArray&& src)
		{
			for (uint32_t i = 0; i < SIZE; ++i)
			{
				new (GetVoid(i)) Type(Move(src[i]));
			}
		}
		StaticArray& operator=(StaticArray&& src)
		{
			if (this != &src)
			{
				for (uint32_t i = 0; i < SIZE; ++i)
				{
					(*this)[i] = Move(src[i]);
				}
			}
			return *this;
		}

		// 開放
		//----------------------

		~StaticArray()
		{
			if constexpr (!IsTriviallyDestructibleV<Type>)
			{
				for (uint32_t i = 0; i < SIZE; ++i)
				{
					Get(i)->~Type();
				}
			}
		}

		// 取得
		//----------------------

		inline Type& operator[](const uint32_t index)
		{
			assert(index < SIZE && "範囲外アクセスです");
			return *Get(index);
		}
		inline const Type& operator[](const uint32_t index)const
		{
			assert(index < SIZE && "範囲外アクセスです");
			return *Get(index);
		}

		/// <summary>
		/// 添え字の要素を取得する
		/// </summary>
		/// <returns>範囲外ならnullptr</returns>
		inline Type* Get(const uint32_t index)
		{
			if (index >= SIZE)
			{
				return nullptr;
			}
			return reinterpret_cast<Type*>(mem_) + index;
		}
		inline const Type* Get(const uint32_t index)const
		{
			if (index >= SIZE)
			{
				return nullptr;
			}
			return reinterpret_cast<const Type*>(mem_) + index;
		}

		inline Type* Data()
		{
			return reinterpret_cast<Type*>(mem_);
		}
		inline const Type* Data()const
		{
			return reinterpret_cast<const Type*>(mem_);
		}

		inline Type* Begin()
		{
			return Data();
		}
		inline const Type* Begin()const
		{
			return Data();
		}
		inline Type* End()
		{
			return Data() + SIZE;
		}
		inline const Type* End()const
		{
			return Data() + SIZE;
		}

		// 範囲forループ用
		inline Type* begin() { return Begin(); }
		inline const Type* begin()const { return Begin(); }
		inline Type* end() { return End(); }
		inline const Type* end()const { return End(); }

		static constexpr uint32_t Size()
		{
			return SIZE;
		}

		inline bool IsValidIndex(const uint32_t index)const
		{
			return index < SIZE;
		}

	private:
		inline void* GetVoid(const uint32_t index)
		{
			return mem_ + sizeof(Type) * index;
		}

	private:
		alignas(Type) unsigned char mem_[sizeof(Type) * SIZE];
	};
}
