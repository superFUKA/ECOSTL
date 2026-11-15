#include "ECOSTL/Core/Platform/Platform.h"

#include <gtest/gtest.h>

#include <cstdint>

namespace
{
	// 別々のスレッドが書く欄の形(キャッシュラインに揃える)
	struct alignas(utl::CACHE_LINE_SIZE) Lane
	{
		uint32_t value = 0;
	};
}

static_assert(utl::CACHE_LINE_SIZE == 64, "x64 の CPU のキャッシュラインは 64 バイト");
static_assert((utl::CACHE_LINE_SIZE & (utl::CACHE_LINE_SIZE - 1)) == 0, "alignas に使えるよう 2 の累乗");
static_assert(alignof(Lane) == utl::CACHE_LINE_SIZE && sizeof(Lane) == utl::CACHE_LINE_SIZE, "揃えた欄は 1 ラインを占める");

TEST(Platform, CacheLineSizeSeparatesNeighbours)
{
	// 並べた欄は、隣どうしが別のラインに乗る
	Lane lanes[2];
	const uintptr_t first = reinterpret_cast<uintptr_t>(&lanes[0]);
	const uintptr_t second = reinterpret_cast<uintptr_t>(&lanes[1]);
	EXPECT_EQ(first % utl::CACHE_LINE_SIZE, 0u);
	EXPECT_EQ(second - first, static_cast<uintptr_t>(utl::CACHE_LINE_SIZE));
}
