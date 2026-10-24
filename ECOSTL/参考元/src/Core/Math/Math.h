#pragma once
#include <cinttypes>

namespace utl
{
	/// <summary>
	/// 渡した引数のうち、小さいほうを返す
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <param name="a"></param>
	/// <param name="b"></param>
	/// <returns></returns>
	template<typename Type>
	inline static constexpr Type Min(Type a, Type b)
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
	inline static constexpr Type Max(Type a, Type b)
	{
		return (a > b) ? a : b;
	}

    /// <summary>
    /// 左に出たビットが右から戻る
    /// </summary>
    /// <typeparam name="T"></typeparam>
    /// <param name="v"></param>
    /// <param name="s"></param>
    /// <returns></returns>
    template<class T>
    constexpr T RotL(T v, uint32_t s) noexcept
    {
        constexpr uint32_t BITS = sizeof(T) * 8u;
        s &= (BITS - 1u);
        return (v << s) | (v >> ((BITS - s) & (BITS - 1u)));
    }
    /// <summary>
    /// 右に出たビットが左から戻る
    /// </summary>
    /// <typeparam name="T"></typeparam>
    /// <param name="v"></param>
    /// <param name="s"></param>
    /// <returns></returns>
    template<class T>
    constexpr T RotR(T v, uint32_t s) noexcept
    {
        constexpr uint32_t BITS = sizeof(T) * 8u;
        s &= (BITS - 1u);
        return (v >> s) | (v << ((BITS - s) & (BITS - 1u)));
    }
}