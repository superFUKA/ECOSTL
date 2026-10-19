#pragma once
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
}
