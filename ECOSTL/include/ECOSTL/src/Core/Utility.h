#pragma once
#include <cstdint>
#include "../Type/TypeTraits.h"

namespace utl
{
	/// <summary>
	/// 値を右辺値として扱う（ムーブしてよいことを示す）
	/// </summary>
	template<typename T>
	constexpr RemoveReferenceType<T>&& Move(T&& value) noexcept
	{
		return static_cast<RemoveReferenceType<T>&&>(value);
	}

	/// <summary>
	/// 受け取ったときの値の種類（左辺値か右辺値か）のまま渡す
	/// </summary>
	template<typename T>
	constexpr T&& Forward(RemoveReferenceType<T>& value) noexcept
	{
		return static_cast<T&&>(value);
	}
	/// <summary>
	/// 受け取ったときの値の種類（左辺値か右辺値か）のまま渡す
	/// </summary>
	template<typename T>
	constexpr T&& Forward(RemoveReferenceType<T>&& value) noexcept
	{
		static_assert(!IsLvalueReferenceV<T>, "右辺値を左辺値として渡そうとしています");
		return static_cast<T&&>(value);
	}

	/// <summary>
	/// 2つの値を入れ替える
	/// </summary>
	template<typename T>
	constexpr void Swap(T& a, T& b) noexcept(noexcept(T(Move(a))) && noexcept(a = Move(b)))
	{
		T tmp(Move(a));
		a = Move(b);
		b = Move(tmp);
	}

	/// <summary>
	/// メモリをコピーする（領域が重なってはいけない）
	/// </summary>
	inline void MemCopy(void* dest, const void* src, const uint64_t byteSize) noexcept
	{
#if defined(__clang__) || defined(__GNUC__)
		__builtin_memcpy(dest, src, static_cast<decltype(sizeof(0))>(byteSize));
#else
		uint8_t* d = static_cast<uint8_t*>(dest);
		const uint8_t* s = static_cast<const uint8_t*>(src);
		for (uint64_t i = 0; i < byteSize; ++i)
		{
			d[i] = s[i];
		}
#endif
	}

	/// <summary>
	/// メモリをコピーする（領域が重なってもよい）
	/// </summary>
	inline void MemMove(void* dest, const void* src, const uint64_t byteSize) noexcept
	{
#if defined(__clang__) || defined(__GNUC__)
		__builtin_memmove(dest, src, static_cast<decltype(sizeof(0))>(byteSize));
#else
		uint8_t* d = static_cast<uint8_t*>(dest);
		const uint8_t* s = static_cast<const uint8_t*>(src);
		if (d < s)
		{
			for (uint64_t i = 0; i < byteSize; ++i)
			{
				d[i] = s[i];
			}
		}
		else
		{
			for (uint64_t i = byteSize; i > 0; --i)
			{
				d[i - 1] = s[i - 1];
			}
		}
#endif
	}

	/// <summary>
	/// メモリを同じバイトの値で埋める
	/// </summary>
	inline void MemSet(void* dest, const uint8_t value, const uint64_t byteSize) noexcept
	{
#if defined(__clang__) || defined(__GNUC__)
		__builtin_memset(dest, value, static_cast<decltype(sizeof(0))>(byteSize));
#else
		uint8_t* d = static_cast<uint8_t*>(dest);
		for (uint64_t i = 0; i < byteSize; ++i)
		{
			d[i] = value;
		}
#endif
	}
}
