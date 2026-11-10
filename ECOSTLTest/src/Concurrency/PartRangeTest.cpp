#include "ECOSTL/Concurrency/PartRange.h"

#include <gtest/gtest.h>

#include <cstdint>

TEST(PartRange, CountParts)
{
	EXPECT_EQ(utl::CountParts(0, 4), 0u);
	EXPECT_EQ(utl::CountParts(1, 4), 1u);
	EXPECT_EQ(utl::CountParts(4, 4), 1u);
	EXPECT_EQ(utl::CountParts(5, 4), 2u);
	EXPECT_EQ(utl::CountParts(8, 4), 2u);
	EXPECT_EQ(utl::CountParts(UINT32_MAX, 1), UINT32_MAX);
	EXPECT_EQ(utl::CountParts(UINT32_MAX, UINT32_MAX), 1u);
}

TEST(PartRange, RangesCoverEverythingOnce)
{
	const uint32_t count = 10;
	const uint32_t partSize = 4;
	ASSERT_EQ(utl::CountParts(count, partSize), 3u);
	uint32_t next = 0;
	for (uint32_t part = 0; part < 3; ++part)
	{
		const utl::PartRange range = utl::MakePartRange(part, partSize, count);
		EXPECT_EQ(range.begin, next);
		EXPECT_GT(range.end, range.begin);
		next = range.end;
	}
	EXPECT_EQ(next, count);
	// 最後の区画は端数
	EXPECT_EQ(utl::MakePartRange(2, partSize, count).Size(), 2u);
}

TEST(PartRange, DoesNotOverflowNearTheLimit)
{
	// begin + partSize が 32 ビットを超える所でも、終わりは count になる
	const uint32_t count = UINT32_MAX;
	const uint32_t partSize = 1u << 31;
	ASSERT_EQ(utl::CountParts(count, partSize), 2u);
	const utl::PartRange last = utl::MakePartRange(1, partSize, count);
	EXPECT_EQ(last.begin, partSize);
	EXPECT_EQ(last.end, count);
}

static_assert(utl::CountParts(9, 3) == 3, "constexpr で使える");
static_assert(utl::MakePartRange(1, 3, 5).end == 5, "constexpr で使える");
