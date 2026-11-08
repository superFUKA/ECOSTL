#pragma once
// 昇順に並んだ配列への二分探索。コンテナに依存せず、先頭のポインタと要素数を受け取る。
#include <cstdint>

// ecoSTL 全体の名前空間。
namespace utl
{
	/// <summary>
	/// 昇順に並んだ配列で、value 以上の値が最初に現れる添え字を求める
	/// T は operator< で比較できること
	/// </summary>
	/// <returns>value 以上の値が無ければ size</returns>
	template<typename T>
	uint32_t LowerBound(const T* values, const uint32_t size, const T& value)
	{
		uint32_t left = 0;
		uint32_t right = size;
		while (left < right)
		{
			const uint32_t mid = left + ((right - left) >> 1);
			if (values[mid] < value)
			{
				left = mid + 1;
			}
			else
			{
				right = mid;
			}
		}
		return left;
	}

	/// <summary>
	/// 昇順に並んだ配列で、value が何番目にあるかを求める
	/// T は operator< と operator== で比較できること
	/// </summary>
	/// <returns>value が無ければ UINT32_MAX</returns>
	template<typename T>
	uint32_t BinaryFind(const T* values, const uint32_t size, const T& value)
	{
		const uint32_t index = LowerBound(values, size, value);
		return index < size && values[index] == value ? index : UINT32_MAX;
	}
}
