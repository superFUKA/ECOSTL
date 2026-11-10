#include "ECOSTL/Concurrency/BucketPartition.h"

#include <gtest/gtest.h>

#include <cstdint>

#include "ECOSTL/Container/Vector/Vector.h"
#include "ECOSTL/Memory/Allocator/CountingAllocator.h"
#include "ECOSTL/Memory/Allocator/FailingAllocator.h"

namespace
{
	// 区画を実行する順
	enum class Order
	{
		Forward,
		Backward,
	};

	template<typename Job>
	void RunParts(const uint32_t partCount, const Order order, Job&& job)
	{
		for (uint32_t i = 0; i < partCount; ++i)
		{
			job(order == Order::Forward ? i : partCount - 1 - i);
		}
	}

	// keys を、キーをバケツの番号として振り分け、並べ直した後の「元の番号」の列を返す
	void Partition(utl::BucketPartition& partition, const utl::Vector<uint32_t>& keys, const uint32_t partSize, const uint32_t bucketCount,
		const Order order, utl::Vector<uint32_t>& outIndices)
	{
		ASSERT_TRUE(partition.Reset(keys.Size(), partSize, bucketCount));
		ASSERT_TRUE(outIndices.Resize(keys.Size()));
		RunParts(partition.PartCount(), order, [&](const uint32_t part)
			{
				uint32_t* counts = partition.BeginCount(part);
				const utl::PartRange range = partition.RangeOfPart(part);
				for (uint32_t i = range.begin; i < range.end; ++i)
				{
					++counts[keys[i]];
				}
			});
		partition.AssignPositions();
		RunParts(partition.PartCount(), order, [&](const uint32_t part)
			{
				uint32_t* cursors = partition.CursorsOfPart(part);
				const utl::PartRange range = partition.RangeOfPart(part);
				for (uint32_t i = range.begin; i < range.end; ++i)
				{
					outIndices[cursors[keys[i]]++] = i;
				}
			});
	}

	void FillKeys(utl::Vector<uint32_t>& keys, const uint32_t count, const uint32_t bucketCount, uint32_t seed)
	{
		keys.Clear();
		for (uint32_t i = 0; i < count; ++i)
		{
			seed = seed * 1664525u + 1013904223u;
			keys.PushBack((seed >> 16) % bucketCount);
		}
	}
}

TEST(BucketPartition, EmptyInput)
{
	utl::BucketPartition partition;
	ASSERT_TRUE(partition.Reset(0, 4, 3));
	EXPECT_EQ(partition.PartCount(), 0u);
	partition.AssignPositions();
	for (uint32_t bucket = 0; bucket < 3; ++bucket)
	{
		EXPECT_EQ(partition.BucketBegin(bucket), 0u);
		EXPECT_EQ(partition.BucketEnd(bucket), 0u);
	}
}

TEST(BucketPartition, SortsByBucketKeepingOriginalOrder)
{
	const uint32_t bucketCount = 5;
	utl::Vector<uint32_t> keys;
	FillKeys(keys, 1000, bucketCount, 7);
	utl::BucketPartition partition;
	utl::Vector<uint32_t> indices;
	Partition(partition, keys, 64, bucketCount, Order::Forward, indices);

	EXPECT_EQ(partition.BucketBegin(0), 0u);
	EXPECT_EQ(partition.BucketEnd(bucketCount - 1), keys.Size());
	EXPECT_EQ(partition.BucketBegin(bucketCount), keys.Size());
	for (uint32_t bucket = 0; bucket < bucketCount; ++bucket)
	{
		ASSERT_LE(partition.BucketBegin(bucket), partition.BucketEnd(bucket));
		if (bucket + 1 < bucketCount)
		{
			EXPECT_EQ(partition.BucketEnd(bucket), partition.BucketBegin(bucket + 1));
		}
		uint32_t expectedCount = 0;
		for (const uint32_t key : keys)
		{
			expectedCount += key == bucket ? 1u : 0u;
		}
		EXPECT_EQ(partition.BucketEnd(bucket) - partition.BucketBegin(bucket), expectedCount);
		for (uint32_t position = partition.BucketBegin(bucket); position < partition.BucketEnd(bucket); ++position)
		{
			// そのバケツの要素だけが、元の番号の小さい順に並ぶ
			EXPECT_EQ(keys[indices[position]], bucket);
			if (position > partition.BucketBegin(bucket))
			{
				EXPECT_LT(indices[position - 1], indices[position]);
			}
		}
	}
}

TEST(BucketPartition, SameResultForAnyPartSizeAndOrder)
{
	const uint32_t bucketCount = 7;
	utl::Vector<uint32_t> keys;
	FillKeys(keys, 777, bucketCount, 123);
	utl::BucketPartition partition;
	utl::Vector<uint32_t> expected;
	// 区画 1 つ（直列）の結果を基準にする
	Partition(partition, keys, 100000, bucketCount, Order::Forward, expected);
	ASSERT_EQ(partition.PartCount(), 1u);

	const uint32_t partSizes[] = { 1, 3, 64, 776, 777 };
	for (const uint32_t partSize : partSizes)
	{
		for (const Order order : { Order::Forward, Order::Backward })
		{
			utl::Vector<uint32_t> actual;
			Partition(partition, keys, partSize, bucketCount, order, actual);
			ASSERT_EQ(actual.Size(), expected.Size());
			for (uint32_t i = 0; i < expected.Size(); ++i)
			{
				ASSERT_EQ(actual[i], expected[i]) << "partSize=" << partSize << " i=" << i;
			}
		}
	}
}

TEST(BucketPartition, ReusesWorkAreaWithoutAllocating)
{
	utl::CountingAllocator counting;
	utl::BucketPartition partition(counting.GetAPI());
	utl::Vector<uint32_t> keys;
	FillKeys(keys, 500, 4, 1);
	utl::Vector<uint32_t> indices;
	Partition(partition, keys, 32, 4, Order::Forward, indices);
	const uint64_t countAfterFirst = counting.GetStats().count;
	EXPECT_GT(countAfterFirst, 0u);

	// 同じ大きさ・小さい大きさでは確保しない
	Partition(partition, keys, 32, 4, Order::Forward, indices);
	FillKeys(keys, 100, 3, 2);
	Partition(partition, keys, 32, 3, Order::Forward, indices);
	EXPECT_EQ(counting.GetStats().count, countAfterFirst);
}

TEST(BucketPartition, ReportsAllocationFailure)
{
	// 何回目の確保で失敗しても、false を返して区画が 0 個になる
	for (uint32_t successCount = 0; successCount < 2; ++successCount)
	{
		utl::FailingAllocator failing(successCount);
		utl::BucketPartition partition(failing.GetAPI());
		EXPECT_FALSE(partition.Reset(100, 8, 4));
		EXPECT_EQ(partition.PartCount(), 0u);
		EXPECT_EQ(partition.Count(), 0u);
	}
	utl::FailingAllocator enough(2);
	utl::BucketPartition partition(enough.GetAPI());
	EXPECT_TRUE(partition.Reset(100, 8, 4));
}

TEST(BucketPartition, RejectsTableLargerThan32Bits)
{
	utl::BucketPartition partition;
	// 区画の個数 × バケツの個数が 32 ビットを超える
	EXPECT_FALSE(partition.Reset(UINT32_MAX, 1, 2));
	EXPECT_EQ(partition.PartCount(), 0u);
}
