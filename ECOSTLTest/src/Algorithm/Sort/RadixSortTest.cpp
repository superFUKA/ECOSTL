// 汎用アルゴリズムの検証。ECSの型や実装は読み込まない。
#include "ECOSTL/Algorithm/Sort/RadixSort.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <vector>

namespace
{
	struct WideRecord { uint32_t serial; uint64_t key; };
	struct Payload
	{
		uint64_t originalIndex;
		uint32_t check;
	};
	struct Entry { uint32_t key; Payload value; };
}

TEST(RadixSort, MemberSortMatchesStableReference)
{
	for (const uint32_t bits : { 8u, 16u })
	for (const uint32_t count : { 0u, 1u, 63u, 64u, 65535u, 65536u, 70000u })
	for (const uint64_t mask : { uint64_t(0), uint64_t(255), uint64_t(0xffff000000000000ULL), UINT64_MAX })
	{
		SCOPED_TRACE(bits);
		SCOPED_TRACE(count);
		SCOPED_TRACE(mask);
		std::vector<WideRecord> values(count), scratch(count);
		std::vector<uint32_t> counts(size_t{ 1 } << bits);
		uint64_t random = 1234567;
		for (uint32_t i = 0; i < count; ++i)
		{
			random = random * 6364136223846793005ULL + 1;
			values[i] = { i, (i % 5 == 0 ? UINT64_MAX : random) & mask };
		}
		auto expected = values;
		std::stable_sort(expected.begin(), expected.end(),
			[](const WideRecord& a, const WideRecord& b) { return a.key < b.key; });
		auto* result = utl::StableRadixSortByMember(values.data(), scratch.data(), count,
			&WideRecord::key, counts.data(), bits);
		for (uint32_t i = 0; i < count; ++i)
		{
			ASSERT_EQ(result[i].key, expected[i].key);
			ASSERT_EQ(result[i].serial, expected[i].serial);
		}
		if (mask == 0 || count < 2) EXPECT_EQ(result, values.data());
		auto* spare = result == values.data() ? scratch.data() : values.data();
		result = utl::StableRadixSortByMember(result, spare, count,
			&WideRecord::key, counts.data(), bits);
		for (uint32_t i = 0; i < count; ++i)
			ASSERT_EQ(result[i].serial, expected[i].serial);
	}
}

// 桁の境界・重複キー・32bit最大値で標準の安定ソートと比較する。
TEST(RadixSort, MatchesStableReferenceAcrossDigitBoundaries)
{
	constexpr uint32_t count = 4097;
	for (const uint32_t bits : { 0u, 1u, 11u, 12u, 22u, 23u, 32u })
	{
		SCOPED_TRACE(bits);
		const uint32_t mask = bits == 32 ? UINT32_MAX : (1u << bits) - 1;
		std::vector<uint32_t> keysA(count), keysB(count);
		std::vector<Payload> valuesA(count), valuesB(count);
		std::vector<Entry> expected;
		uint32_t random = 1234567;
		for (uint32_t i = 0; i < count; ++i)
		{
			random = random * 1664525u + 1013904223u;
			keysA[i] = (i % 5 == 0 ? mask : random) & mask;
			valuesA[i] = { i, i ^ 0xabcdu };
			expected.push_back({ keysA[i], valuesA[i] });
		}
		std::stable_sort(expected.begin(), expected.end(), [](const Entry& a, const Entry& b) { return a.key < b.key; });
		auto* keys = keysA.data();
		auto* values = valuesA.data();
		auto* scratchKeys = keysB.data();
		auto* scratchValues = valuesB.data();
		utl::StableRadixSortPairs(keys, values, scratchKeys, scratchValues, count, bits);
		for (uint32_t i = 0; i < count; ++i)
		{
			ASSERT_EQ(keys[i], expected[i].key);
			ASSERT_EQ(values[i].originalIndex, expected[i].value.originalIndex);
			ASSERT_EQ(values[i].check, expected[i].value.check);
		}
		const bool oddPasses = ((bits + 10) / 11) % 2 != 0;
		EXPECT_EQ(keys, oddPasses ? keysB.data() : keysA.data());
		EXPECT_EQ(values, oddPasses ? valuesB.data() : valuesA.data());
		EXPECT_EQ(scratchKeys, oddPasses ? keysA.data() : keysB.data());
	}
}

TEST(RadixSort, EmptyAndSingleDoNotSwapBuffers)
{
	uint32_t* emptyKeys = nullptr;
	uint32_t* emptyValues = nullptr;
	uint32_t* emptyScratchKeys = nullptr;
	uint32_t* emptyScratchValues = nullptr;
	utl::StableRadixSortPairs(emptyKeys, emptyValues, emptyScratchKeys, emptyScratchValues, 0);
	EXPECT_EQ(emptyKeys, nullptr);
	uint32_t key = 7, value = 19, scratchKey = 99, scratchValue = 88;
	auto* keys = &key;
	auto* values = &value;
	auto* scratchKeys = &scratchKey;
	auto* scratchValues = &scratchValue;
	utl::StableRadixSortPairs(keys, values, scratchKeys, scratchValues, 1);
	EXPECT_EQ(keys, &key);
	EXPECT_EQ(values, &value);
	EXPECT_EQ(scratchKey, 99u);
	EXPECT_EQ(scratchValue, 88u);
}

TEST(RadixSort, ReturnedBuffersCanBeReused)
{
	uint32_t keysA[] = { 3, 1, 3, 0 }, keysB[4] = {};
	uint32_t valuesA[] = { 0, 1, 2, 3 }, valuesB[4] = {};
	auto* keys = keysA;
	auto* values = valuesA;
	auto* scratchKeys = keysB;
	auto* scratchValues = valuesB;
	for (int iteration = 0; iteration < 3; ++iteration)
	{
		utl::StableRadixSortPairs(keys, values, scratchKeys, scratchValues, 4, 2);
		const uint32_t expectedKeys[] = { 0, 1, 3, 3 };
		const uint32_t expectedValues[] = { 3, 1, 0, 2 };
		for (uint32_t i = 0; i < 4; ++i)
		{
			EXPECT_EQ(keys[i], expectedKeys[i]);
			EXPECT_EQ(values[i], expectedValues[i]);
		}
	}
}
