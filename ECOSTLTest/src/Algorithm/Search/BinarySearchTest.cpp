// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/Algorithm/Search/BinarySearch.h"

#include <gtest/gtest.h>

#include <cstdint>

// LowerBound は value 以上の値が最初に現れる添え字。無ければ要素数
TEST(BinarySearchTest, LowerBound)
{
	const int values[] = { 1, 3, 3, 5, 9 };
	EXPECT_EQ(utl::LowerBound(values, 5u, 0), 0u);
	EXPECT_EQ(utl::LowerBound(values, 5u, 1), 0u);
	EXPECT_EQ(utl::LowerBound(values, 5u, 3), 1u);	// 同じ値が並ぶときは先頭
	EXPECT_EQ(utl::LowerBound(values, 5u, 4), 3u);
	EXPECT_EQ(utl::LowerBound(values, 5u, 9), 4u);
	EXPECT_EQ(utl::LowerBound(values, 5u, 10), 5u);
	EXPECT_EQ(utl::LowerBound<int>(nullptr, 0u, 1), 0u);	// 空の配列は読まない
}

// BinaryFind は value の添え字。無ければ UINT32_MAX
TEST(BinarySearchTest, BinaryFind)
{
	const uint32_t values[] = { 2, 4, 8, 16, 32 };
	for (uint32_t index = 0; index < 5; ++index)
	{
		EXPECT_EQ(utl::BinaryFind(values, 5u, values[index]), index);
	}
	EXPECT_EQ(utl::BinaryFind(values, 5u, 0u), UINT32_MAX);
	EXPECT_EQ(utl::BinaryFind(values, 5u, 5u), UINT32_MAX);
	EXPECT_EQ(utl::BinaryFind(values, 5u, 64u), UINT32_MAX);
	EXPECT_EQ(utl::BinaryFind<uint32_t>(nullptr, 0u, 2u), UINT32_MAX);
}

// 要素が多くても、すべての値を見つけ、間の値は見つけない
TEST(BinarySearchTest, ManyValues)
{
	uint32_t values[1000];
	for (uint32_t index = 0; index < 1000; ++index)
	{
		values[index] = index * 3;
	}
	for (uint32_t index = 0; index < 1000; ++index)
	{
		EXPECT_EQ(utl::BinaryFind(values, 1000u, index * 3), index);
		EXPECT_EQ(utl::BinaryFind(values, 1000u, index * 3 + 1), UINT32_MAX);
	}
}
