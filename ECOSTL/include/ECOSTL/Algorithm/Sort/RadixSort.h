#pragma once
// uint32キーと付随する値、またはuint64メンバーを、呼び出し側の作業領域で安定に並べ替える。
#include "ECOSTL/Core/Template/Template.h"
#include <cassert>
#include <cstdint>
#include <cstring>

namespace utl
{
	// uint64メンバーをキーに昇順で安定ソートする。全要素で等しい桁は省く。
	// values/scratchは互いに重ならず、構築済みのnum要素を持つこと。
	// countsは独立した2^digitBits要素の作業領域。digitBitsは8または16。
	// 戻り値が結果の配列。元の配列へ戻すコピーやヒープ確保は行わない。
	template<typename T>
	inline T* StableRadixSortByMember(T* values, T* scratch, const uint32_t num,
		uint64_t T::* key, uint32_t* counts, const uint32_t digitBits = 8)
	{
		static_assert(utl::IsTriviallyCopyableV<T>, "Radix sort records must be trivially copyable");
		assert(digitBits == 8 || digitBits == 16);
		if (num < 2) return values;
		const uint32_t bucketNum = 1u << digitBits;
		const uint64_t mask = bucketNum - 1;
		uint64_t differentBits = 0;
		for (uint32_t i = 1; i < num; ++i)
			differentBits |= (values[i].*key) ^ (values[0].*key);
		T* src = values;
		T* dst = scratch;
		for (uint32_t shift = 0; shift < 64; shift += digitBits)
		{
			if (((differentBits >> shift) & mask) == 0) continue;
			std::memset(counts, 0, sizeof(uint32_t) * bucketNum);
			for (uint32_t i = 0; i < num; ++i)
				++counts[((src[i].*key) >> shift) & mask];
			uint32_t offset = 0;
			for (uint32_t bucket = 0; bucket < bucketNum; ++bucket)
			{
				const uint32_t count = counts[bucket];
				counts[bucket] = offset;
				offset += count;
			}
			for (uint32_t i = 0; i < num; ++i)
				dst[counts[((src[i].*key) >> shift) & mask]++] = src[i];
			utl::Swap(src, dst);
		}
		return src;
	}

	// 昇順の安定基数ソート。ECSやアロケータには依存せず、ヒープ確保を行わない。
	// 4つの配列は互いに重ならない領域で、各num要素を保持できること。
	// keyBitsは全キーの値が収まるビット数（0..32）。0なら全キーが0であること。
	// 終了後のkeys/valuesが結果、scratchKeys/scratchValuesが再利用可能な作業領域を指す。
	// ポインタを交換するため、結果は元の配列に戻るとは限らない。num<2なら交換しない。
	template<typename ValueType>
	inline void StableRadixSortPairs(uint32_t*& keys, ValueType*& values,
		uint32_t*& scratchKeys, ValueType*& scratchValues,
		const uint32_t num, const uint32_t keyBits = 32)
	{
		static_assert(utl::IsTriviallyCopyableV<ValueType>, "Radix sort payload must be trivially copyable");
		assert(keyBits <= 32);
		if (num < 2 || keyBits == 0)
		{
			return;
		}

		// 一桁は11bit。度数表は8KiB、32bitのキーは3段で処理する。
		constexpr uint32_t DIGIT_BITS = 11;
		constexpr uint32_t DIGIT_SIZE = 1U << DIGIT_BITS;
		uint32_t histogram[DIGIT_SIZE];
		for (uint32_t shift = 0; shift < keyBits; shift += DIGIT_BITS)
		{
			std::memset(histogram, 0, sizeof(histogram));
			for (uint32_t i = 0; i < num; ++i)
			{
				++histogram[(keys[i] >> shift) & (DIGIT_SIZE - 1)];
			}
			uint32_t sum = 0;
			for (uint32_t digit = 0; digit < DIGIT_SIZE; ++digit)
			{
				const uint32_t count = histogram[digit];
				histogram[digit] = sum;
				sum += count;
			}
			for (uint32_t i = 0; i < num; ++i)
			{
				const uint32_t destination = histogram[(keys[i] >> shift) & (DIGIT_SIZE - 1)]++;
				scratchKeys[destination] = keys[i];
				scratchValues[destination] = values[i];
			}
			utl::Swap(keys, scratchKeys);
			utl::Swap(values, scratchValues);
		}
	}
}
