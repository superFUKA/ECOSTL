#pragma once
// メモリの塊を写す・ずらす・埋める関数。中身は C 標準の memcpy/memmove/memset。
// MSVC には __builtin_memcpy が無く、自前のループにすると遅くなる（コンパイラは標準の memcpy を専用の命令に置き換える）ため、
// 「自前で作るのが現実的でない」ものとして標準を使う（ECOSTL dev の MemCopy は MSVC で1バイトずつのループだった）。
#include <cstdint>
#include <cstring>

// 汎用ユーティリティ名前空間
namespace utl
{
	/// <summary>src から dest へ byteSize バイト写す。範囲が重なってはいけない</summary>
	inline void MemCopy(void* dest, const void* src, const uint64_t byteSize) noexcept
	{
		::memcpy(dest, src, static_cast<size_t>(byteSize));
	}

	/// <summary>src から dest へ byteSize バイト写す。範囲が重なってもよい</summary>
	inline void MemMove(void* dest, const void* src, const uint64_t byteSize) noexcept
	{
		::memmove(dest, src, static_cast<size_t>(byteSize));
	}

	/// <summary>dest から byteSize バイトを value で埋める</summary>
	inline void MemSet(void* dest, const uint8_t value, const uint64_t byteSize) noexcept
	{
		::memset(dest, value, static_cast<size_t>(byteSize));
	}
}
