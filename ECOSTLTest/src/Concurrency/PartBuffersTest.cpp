#include "ECOSTL/Concurrency/PartBuffers.h"

#include <gtest/gtest.h>

#include <cstdint>

#include "ECOSTL/Memory/Allocator/CountingAllocator.h"
#include "ECOSTL/Memory/Allocator/FailingAllocator.h"

namespace
{
	// 区画 part の仕事：part * 10 から始まる連番を countOfPart(part) 個書く
	template<typename CountOfPart>
	void WritePart(utl::PartBuffers<uint32_t>& buffers, const uint32_t part, CountOfPart&& countOfPart)
	{
		uint32_t* out = buffers.BufferOfPart(part);
		const uint32_t capacity = buffers.CapacityOfPart(part);
		const uint32_t count = countOfPart(part);
		uint32_t found = 0;
		for (uint32_t i = 0; i < count; ++i)
		{
			if (found < capacity)
			{
				out[found] = part * 10 + i;
			}
			++found;
		}
		buffers.SetNeeded(part, found);
	}

	// 集める流れを最後まで行う（段 → 広げる → 足りなかった区画だけやり直す）。やり直した区画の個数を返す
	template<typename CountOfPart>
	uint32_t Collect(utl::PartBuffers<uint32_t>& buffers, const uint32_t partCount, CountOfPart&& countOfPart)
	{
		EXPECT_TRUE(buffers.Reset(partCount));
		// 段の仕事は順を問わないので、後ろから実行する
		for (uint32_t part = partCount; part > 0; --part)
		{
			WritePart(buffers, part - 1, countOfPart);
		}
		EXPECT_TRUE(buffers.GrowOverflowedParts());
		const uint32_t redone = buffers.OverflowedParts().Size();
		for (const uint32_t part : buffers.OverflowedParts())
		{
			WritePart(buffers, part, countOfPart);
		}
		return redone;
	}

	template<typename CountOfPart>
	void ExpectContents(const utl::PartBuffers<uint32_t>& buffers, const uint32_t partCount, CountOfPart&& countOfPart)
	{
		ASSERT_EQ(buffers.PartCount(), partCount);
		uint64_t total = 0;
		for (uint32_t part = 0; part < partCount; ++part)
		{
			const uint32_t count = countOfPart(part);
			total += count;
			ASSERT_EQ(buffers.NeededOfPart(part), count);
			ASSERT_GE(buffers.CapacityOfPart(part), count);
			for (uint32_t i = 0; i < count; ++i)
			{
				ASSERT_EQ(buffers.BufferOfPart(part)[i], part * 10 + i);
			}
		}
		EXPECT_EQ(buffers.CountNeeded(), total);
	}
}

TEST(PartBuffers, EmptyParts)
{
	utl::PartBuffers<uint32_t> buffers;
	ASSERT_TRUE(buffers.Reset(0));
	EXPECT_EQ(buffers.PartCount(), 0u);
	EXPECT_TRUE(buffers.GrowOverflowedParts());
	EXPECT_EQ(buffers.OverflowedParts().Size(), 0u);
	EXPECT_EQ(buffers.CountNeeded(), 0u);
}

TEST(PartBuffers, RedoesOnlyOverflowedParts)
{
	utl::PartBuffers<uint32_t> buffers;
	const auto counts = [](const uint32_t part) { return part % 2 == 0 ? 0u : part * 3; };
	// 最初は全ての場所が空なので、結果のある区画（奇数）だけやり直す
	EXPECT_EQ(Collect(buffers, 6, counts), 3u);
	EXPECT_EQ(buffers.OverflowedParts()[0], 1u);
	EXPECT_EQ(buffers.OverflowedParts()[1], 3u);
	EXPECT_EQ(buffers.OverflowedParts()[2], 5u);
	ExpectContents(buffers, 6, counts);
}

TEST(PartBuffers, SettlesWithoutRedoOrAllocation)
{
	utl::CountingAllocator counting;
	utl::PartBuffers<uint32_t> buffers(counting.GetAPI());
	const auto counts = [](const uint32_t part) { return 20 + part; };
	EXPECT_EQ(Collect(buffers, 8, counts), 8u);
	const uint64_t allocations = counting.GetStats().count;

	// 同じ個数なら、やり直しも確保も起きない
	EXPECT_EQ(Collect(buffers, 8, counts), 0u);
	ExpectContents(buffers, 8, counts);
	// 少し増えても、余分に取ってある分に収まる
	const auto slightlyMore = [](const uint32_t part) { return 24 + part; };
	EXPECT_EQ(Collect(buffers, 8, slightlyMore), 0u);
	ExpectContents(buffers, 8, slightlyMore);
	// 区画が減ってから元に戻っても、場所は残っている
	EXPECT_EQ(Collect(buffers, 2, counts), 0u);
	EXPECT_EQ(Collect(buffers, 8, counts), 0u);
	EXPECT_EQ(counting.GetStats().count, allocations);
}

TEST(PartBuffers, GrowsAgainWhenMuchMoreIsNeeded)
{
	utl::PartBuffers<uint32_t> buffers;
	const auto few = [](const uint32_t) { return 4u; };
	const auto many = [](const uint32_t part) { return part == 2 ? 500u : 4u; };
	EXPECT_EQ(Collect(buffers, 4, few), 4u);
	EXPECT_EQ(Collect(buffers, 4, many), 1u);
	EXPECT_EQ(buffers.OverflowedParts()[0], 2u);
	ExpectContents(buffers, 4, many);
}

TEST(PartBuffers, ClearKeepsBuffers)
{
	utl::CountingAllocator counting;
	utl::PartBuffers<uint32_t> buffers(counting.GetAPI());
	const auto counts = [](const uint32_t) { return 10u; };
	Collect(buffers, 3, counts);
	const uint64_t allocations = counting.GetStats().count;
	buffers.Clear();
	EXPECT_EQ(buffers.PartCount(), 0u);
	EXPECT_EQ(buffers.OverflowedParts().Size(), 0u);
	EXPECT_EQ(buffers.CountNeeded(), 0u);
	EXPECT_EQ(Collect(buffers, 3, counts), 0u);
	EXPECT_EQ(counting.GetStats().count, allocations);
}

TEST(PartBuffers, ReportsAllocationFailure)
{
	// Reset の途中のどの確保で失敗しても、false を返して区画が 0 個になる
	uint32_t successCount = 0;
	for (;; ++successCount)
	{
		utl::FailingAllocator failing(successCount);
		utl::PartBuffers<uint32_t> buffers(failing.GetAPI());
		if (buffers.Reset(5))
		{
			EXPECT_EQ(buffers.PartCount(), 5u);
			break;
		}
		EXPECT_EQ(buffers.PartCount(), 0u);
		ASSERT_LT(successCount, 16u);
	}
	EXPECT_GT(successCount, 0u);

	// 広げるときの確保の失敗も伝える
	utl::FailingAllocator failing(successCount);
	utl::PartBuffers<uint32_t> buffers(failing.GetAPI());
	ASSERT_TRUE(buffers.Reset(5));
	buffers.SetNeeded(3, 100);
	EXPECT_FALSE(buffers.GrowOverflowedParts());
}
