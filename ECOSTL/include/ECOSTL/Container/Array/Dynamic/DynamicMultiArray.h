#pragma once
// 複数型の配列を1ブロックに並べる DynamicMultiArray と、その配置計算用ヘルパー
#include "ECOSTL/Memory/Allocator/MemoryAllocator.h"
#include "ECOSTL/TypeInfo/TypeID/TypeID.h"

// ecoSTL の共通名前空間
namespace utl
{
	// 型 T のサイズを alignof の倍数に切り上げたバイト数(配列内で各型を詰める際の1要素分の幅)
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


	// テンプレート引数の全型について AlignedSize を合計したバイト数(1要素分の全型の幅)
	template<typename...Types>
	inline uint32_t TypesSize()
	{
		uint32_t result = 0;

		(AddTypeSize<Types>(result), ...);

		return result;
	}

	// begin+index から num 個ぶん Type をデフォルト構築し、index を進める(placement new、確保はしない)
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
	// begin+index から num 個ぶん src のコピーで Type を構築し、index を進める(placement new、確保はしない)
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

	// begin+index から num 個ぶんの Type のデストラクタを呼び、index を進める(メモリ自体は解放しない)
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



	// 型 CheckType が SubType と同じなら typeIndex を減らし、0 になるまで index を型領域 num 個分ずつ進める(typeIndex 番目の同型の先頭位置を求める折り畳み用)
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

	// 複数型の配列を1つのメモリブロックに型ごとに連続して並べる(SoA)コンテナ。要素数は構築時に固定。スレッドセーフではない
	template<typename...Types>
	class DynamicMultiArray
	{
	public:
		DynamicMultiArray(const uint32_t size, MemoryAllocatorDelegate* alloc = nullptr)
			: alloc_(alloc)
			, begin_(alloc_.Allocate(TypesSize<Types...>()* size))
			, size_(size)
		{
			uint32_t index = 0;
			(InitializeTypeMemories<Types>(static_cast<char*>(begin_), index, size), ...);
		}

		DynamicMultiArray(const uint32_t size, MemoryAllocatorDelegate* alloc, Types&&...srces)
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


		// typeIndex 番目(1始まり)に現れる Type の配列の先頭を返す。O(型数)
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

		// typeIndex 番目の Type の配列の index 番目の要素を返す。範囲チェックはしない
		template<typename Type>
		Type* Get(uint32_t index, uint32_t typeIndex = 1)
		{
			Type* begin = GetBegin<Type>(typeIndex);

			return begin + index;
		}




	private:

		MemoryAllocatorRef alloc_; // 確保・解放に使うアロケータ

		// 全型ぶんを含む確保済みブロックの先頭
		void* begin_;

		// 各型の要素数(全型で共通)
		uint32_t size_;
	};
}
