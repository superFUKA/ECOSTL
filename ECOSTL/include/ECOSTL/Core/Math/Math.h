#pragma once
// 型を選ばない小さな数学ユーティリティ(すべて constexpr)。Min/Max・絶対値・範囲に収める・補間・ビット回転・円周率。
// ベクトル・クォータニオン・行列は Math 層(ECOSTL/Math)にある。
#include <cstdint>

#include "ECOSTL/Core/Template/TypeTraits.h"

// ecoSTL 全体の名前空間。
namespace utl
{
	// 円周率(float)
	inline constexpr float PI = 3.14159265358979f;
	// 2π(1周のラジアン)
	inline constexpr float TAU = 6.28318530717959f;

	/// <summary>
	/// 渡した引数のうち、小さいほうを返す
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <param name="a"></param>
	/// <param name="b"></param>
	/// <returns></returns>
	template<typename Type>
	constexpr Type Min(Type a, Type b)
	{
		return (a < b) ? a : b;
	}
	/// <summary>
	/// 渡した引数のうち、大きいほうを返す
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <param name="a"></param>
	/// <param name="b"></param>
	/// <returns></returns>
	template<typename Type>
	constexpr Type Max(Type a, Type b)
	{
		return (a > b) ? a : b;
	}

	/// <summary>
	/// 絶対値を返す
	/// </summary>
	/// <typeparam name="Type">符号付きの整数か浮動小数点数</typeparam>
	/// <param name="v"></param>
	/// <returns></returns>
	template<typename Type>
	constexpr Type Abs(Type v)
	{
		return (v < static_cast<Type>(0)) ? -v : v;
	}

	/// <summary>
	/// v を lo..hi の範囲に収める。lo <= hi が前提
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <param name="v"></param>
	/// <param name="lo">下限</param>
	/// <param name="hi">上限</param>
	/// <returns></returns>
	template<typename Type>
	constexpr Type Clamp(Type v, Type lo, Type hi)
	{
		return (v < lo) ? lo : ((v > hi) ? hi : v);
	}

	/// <summary>
	/// v を 0..1 の範囲に収める
	/// </summary>
	/// <typeparam name="Type">浮動小数点数</typeparam>
	/// <param name="v"></param>
	/// <returns></returns>
	template<typename Type>
	constexpr Type Saturate(Type v)
	{
		static_assert(IsFloatingPointV<Type>, "Saturate は浮動小数点数だけに使えます");
		return Clamp(v, static_cast<Type>(0), static_cast<Type>(1));
	}

	/// <summary>
	/// a から b への線形補間。t=0 で a、t=1 で b(t は範囲に収めない)
	/// </summary>
	/// <typeparam name="Type">浮動小数点数</typeparam>
	/// <param name="a"></param>
	/// <param name="b"></param>
	/// <param name="t"></param>
	/// <returns></returns>
	template<typename Type>
	constexpr Type Lerp(Type a, Type b, Type t)
	{
		static_assert(IsFloatingPointV<Type>, "Lerp は浮動小数点数だけに使えます");
		return a + (b - a) * t;
	}

	/// <summary>
	/// x が a..b を動くとき 0..1 を滑らかに動く補間(エルミート)。範囲の外は 0 か 1。a != b が前提
	/// </summary>
	/// <typeparam name="Type">浮動小数点数</typeparam>
	/// <param name="a">0 になる位置</param>
	/// <param name="b">1 になる位置</param>
	/// <param name="x"></param>
	/// <returns></returns>
	template<typename Type>
	constexpr Type SmoothStep(Type a, Type b, Type x)
	{
		static_assert(IsFloatingPointV<Type>, "SmoothStep は浮動小数点数だけに使えます");
		const Type t = Saturate((x - a) / (b - a));
		return t * t * (static_cast<Type>(3) - static_cast<Type>(2) * t);
	}

	/// <summary>
	/// 左に出たビットが右から戻る
	/// </summary>
	/// <typeparam name="T">符号なしの整数(bool を除く)。符号付きは右シフトの結果が処理系で変わるため受け付けない</typeparam>
	/// <param name="v"></param>
	/// <param name="s">回す量(ビット数以上は剰余を取る)</param>
	/// <returns></returns>
	template<class T>
	constexpr T RotL(T v, uint32_t s) noexcept
	{
		static_assert(IsUnsignedV<T> && !IsSameV<RemoveCVType<T>, bool>, "RotL は符号なしの整数だけに使えます");
		constexpr uint32_t BITS = sizeof(T) * 8u;
		s &= (BITS - 1u);
		return static_cast<T>((v << s) | (v >> ((BITS - s) & (BITS - 1u))));
	}
	/// <summary>
	/// 右に出たビットが左から戻る
	/// </summary>
	/// <typeparam name="T">符号なしの整数(bool を除く)。符号付きは右シフトの結果が処理系で変わるため受け付けない</typeparam>
	/// <param name="v"></param>
	/// <param name="s">回す量(ビット数以上は剰余を取る)</param>
	/// <returns></returns>
	template<class T>
	constexpr T RotR(T v, uint32_t s) noexcept
	{
		static_assert(IsUnsignedV<T> && !IsSameV<RemoveCVType<T>, bool>, "RotR は符号なしの整数だけに使えます");
		constexpr uint32_t BITS = sizeof(T) * 8u;
		s &= (BITS - 1u);
		return static_cast<T>((v >> s) | (v << ((BITS - s) & (BITS - 1u))));
	}
}
