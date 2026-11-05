#pragma once
// 先頭 Inline 個を自分の中に持ち、超えたらヒープへ移る可変長配列(TrivialInlineArray<T, Inline>)。
//  - ゼロ初期化の状態が「空」なので、memset(0) した領域をそのまま空の配列として使える。
//  - 平凡にコピーできる型(デストラクタを持たない)にしてある。memcpy で移動・並べ替えができる。
//    その代わり、ヒープを使っている間は捨てる前に Release() を呼ぶこと(呼ばないとヒープが漏れる)。Inline 以内なら何も要らない。
//  - コピーは浅い(ヒープを使っている間のコピーは同じヒープを指す)。コピーした側のどちらかだけを Release/書き換えること。
//  - 同じ配列を複数のスレッドから同時に書かない(読みは並列でよい)。
//  - ゼロ埋めで空になる性質を保つため、アロケータはメンバーに持たず、確保・開放する関数に引数で渡す(DI)。
//    同じ配列には、毎回同じアロケータを渡すこと。省略すると malloc/free。
#include "ECOSTL/Core/Template/Template.h"
#include <cstdint>
#include <cstring>
#include "ECOSTL/Memory/Allocator/MemoryAllocator.h"

// ecoSTL の共通名前空間
namespace utl
{
	template<typename T, uint32_t Inline>
	struct TrivialInlineArray
	{
		static_assert(utl::IsTriviallyCopyableV<T>, "TrivialInlineArray の要素は平凡にコピーできる型にすること");
		static_assert(Inline > 0, "Inline は 1 以上");

		uint32_t size;       // 今の要素数
		uint32_t heapCap;    // 0 = 自分の中(Inline 個まで)。0 でなければヒープの容量(要素数)
		union
		{
			T inl[Inline];
			T* heap;
		};

		uint32_t Size() const { return size; }
		bool IsEmpty() const { return size == 0; }
		uint32_t Capacity() const { return heapCap != 0 ? heapCap : Inline; }
		bool UsesHeap() const { return heapCap != 0; }
		T* Data() { return heapCap != 0 ? heap : inl; }
		const T* Data() const { return heapCap != 0 ? heap : inl; }
		T& operator[](const uint32_t i) { return Data()[i]; }
		const T& operator[](const uint32_t i) const { return Data()[i]; }
		T* Begin() { return Data(); }
		T* End() { return Data() + size; }
		const T* Begin() const { return Data(); }
		const T* End() const { return Data() + size; }

		/// <summary>容量を n 個以上にする(Inline を超えるときだけヒープを確保する)。失敗(メモリ不足)なら false</summary>
		bool Reserve(const uint32_t n, MemoryAllocatorRef alloc = MemoryAllocatorRef())
		{
			if (n <= Capacity()) { return true; }
			T* mem = static_cast<T*>(alloc.Allocate(static_cast<uint32_t>(sizeof(T)) * n, static_cast<uint32_t>(alignof(T) < alignof(void*) ? alignof(void*) : alignof(T))));
			if (mem == nullptr) { return false; }
			std::memcpy(mem, Data(), sizeof(T) * size);
			if (heapCap != 0) { alloc.Free(heap); }
			heap = mem;
			heapCap = n;
			return true;
		}

		bool PushBack(const T& v, MemoryAllocatorRef alloc = MemoryAllocatorRef())
		{
			if (size == Capacity() && !Reserve(size == 0 ? Inline * 2 : size * 2, alloc)) { return false; }
			Data()[size++] = v;
			return true;
		}

		/// <summary>index の要素を消して、最後の要素を入れる(順序は保たない)</summary>
		void SwapRemoveAt(const uint32_t index)
		{
			Data()[index] = Data()[size - 1];
			--size;
		}

		/// <summary>value と等しい最初の要素を SwapRemoveAt で消す。見つからなければ false</summary>
		bool SwapRemove(const T& value)
		{
			for (uint32_t i = 0; i < size; ++i)
			{
				if (Data()[i] == value) { SwapRemoveAt(i); return true; }
			}
			return false;
		}

		void Clear() { size = 0; }

		/// <summary>ヒープを返して空にする(捨てる前に呼ぶ。Inline 以内なら何もしない)</summary>
		void Release(MemoryAllocatorRef alloc = MemoryAllocatorRef())
		{
			if (heapCap != 0) { alloc.Free(heap); heapCap = 0; }
			size = 0;
		}
	};
}
