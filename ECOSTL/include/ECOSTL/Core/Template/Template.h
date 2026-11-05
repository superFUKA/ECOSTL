#pragma once
// <utility> に頼らない Move/Forward/Swap と、整数の並び（IndexSequence）。型特性は TypeTraits.h。
#include "ECOSTL/Core/Template/TypeTraits.h"

// 汎用ユーティリティ名前空間
namespace utl
{
	//----------------------------
	// 移動・転送
	//----------------------------

	/// <summary>
	/// 値を右辺値参照にする（std::move の代わり）。value の中身は移動元として扱われる
	/// </summary>
	template<typename T>
	constexpr RemoveReferenceType<T>&& Move(T&& value) noexcept
	{
		return static_cast<RemoveReferenceType<T>&&>(value);
	}

	/// <summary>
	/// 受け取った引数を、元の値の種類（左辺値・右辺値）のまま渡す（std::forward の代わり）
	/// </summary>
	template<typename T>
	constexpr T&& Forward(RemoveReferenceType<T>& value) noexcept
	{
		return static_cast<T&&>(value);
	}
	template<typename T>
	constexpr T&& Forward(RemoveReferenceType<T>&& value) noexcept
	{
		static_assert(!IsLvalueReferenceV<T>, "右辺値を左辺値として渡そうとしています");
		return static_cast<T&&>(value);
	}

	/// <summary>
	/// 2つの値を入れ替える（std::swap の代わり）
	/// </summary>
	template<typename T>
	constexpr void Swap(T& a, T& b) noexcept(noexcept(T(Move(a))) && noexcept(a = Move(b)))
	{
		T tmp(Move(a));
		a = Move(b);
		b = Move(tmp);
	}

	//----------------------------
	// 整数の並び（std::index_sequence の代わり）
	//----------------------------

	template<typename T, T...Values>
	struct IntegerSequence
	{
		static constexpr decltype(sizeof(0)) Size()noexcept
		{
			return sizeof...(Values);
		}
	};

	template<decltype(sizeof(0))...Indices>
	using IndexSequence = IntegerSequence<decltype(sizeof(0)), Indices...>;

	// 0..N-1 の並び。コンパイラの組み込みで作る（MSVC・clang は __make_integer_seq、GCC は __integer_pack）
#if defined(_MSC_VER) || defined(__clang__)
	template<typename T, T N>
	using MakeIntegerSequence = __make_integer_seq<IntegerSequence, T, N>;
#else
	template<typename T, T N>
	using MakeIntegerSequence = IntegerSequence<T, __integer_pack(N)...>;
#endif

	template<decltype(sizeof(0)) N>
	using MakeIndexSequence = MakeIntegerSequence<decltype(sizeof(0)), N>;

	template<typename...Types>
	using IndexSequenceFor = MakeIndexSequence<sizeof...(Types)>;
}
