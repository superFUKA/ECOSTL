#pragma once
#include "Vector.h"

namespace utl
{
	//-------------------------------------------------------------
	// サイズ計算
	//-------------------------------------------------------------

	/// <summary>
	/// アライメントした後の型のサイズをコンパイル時に求めて返す
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <returns></returns>
	template<class T>
	constexpr size_t AlignedSize()
	{
		constexpr size_t a = alignof(T);
		return (sizeof(T) + a - 1) / a * a;
	}


	/// <summary>
	/// 引数にアライメントされた型のサイズを足す
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <param name="result"></param>
	template<typename Type>
	inline void AddTypeSize(size_t& result)
	{
		result += AlignedSize<Type>();
	}

	/// <summary>
	/// 複数のアライメントされた型のサイズの合計を求める
	/// </summary>
	/// <typeparam name="...Types"></typeparam>
	/// <returns></returns>
	template<typename...Types>
	inline size_t TypesSize()
	{
		size_t result = 0;

		(AddTypeSize<Types>(result), ...);

		return result;
	}



	//-------------------------------------------------------------
	// 複数の型
	//-------------------------------------------------------------

	/// <summary>
	/// 
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <param name="begin"></param>
	/// <param name="index"></param>
	/// <param name="num"></param>
	template<typename Type>
	inline void InitializeTypeMemories(char* begin, size_t& index, size_t beginIndex, size_t num)
	{
		for (size_t i = 0; i < num; ++i)
		{
			void* voidPtr = static_cast<void*>(begin + (index * i));
			Type* ptr = static_cast<Type*>(voidPtr);
			new (ptr) Type();
			index += AlignedSize<Type>();
		}
	}
	/// <summary>
	/// 
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <param name="begin"></param>
	/// <param name="index"></param>
	/// <param name="num"></param>
	/// <param name="src"></param>
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
	class MultiVector
	{
	public:
		MultiVector(MemoryAllocatorAPI* alloc)
			:alloc_(alloc)
		{
		}

		void PushBack(const Types&...args)
		{
			size_t index = size_;
			(InitializeTypeMemories<Types>(static_cast<char*>(begin_), index, size_, args), ...);
		}

		void PushBack()
		{
			size_t index = size_;
			(InitializeTypeMemories<Types>(static_cast<char*>(begin_), index, size_, args), ...);
		}

		void Resize(const size_t index, Types&&...srcs)
		{
			for (size_t i = 0; i < index; i++)
			{
				size_t index = size_;
				(InitializeTypeMemories<Types>(static_cast<char*>(begin_), index, size_, args), ...);
			}
		}

	private:


		MemoryAllocatorAPIHolder alloc_;

		void* begin_;

		size_t size_;




	};
}
