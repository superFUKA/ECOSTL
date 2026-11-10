#include "ECOSTL/Memory/Allocator/FailingAllocator.h"

#include <gtest/gtest.h>

#include <cstdint>

#include "ECOSTL/Container/Vector/Vector.h"
#include "ECOSTL/Memory/Allocator/CountingAllocator.h"

TEST(FailingAllocator, SucceedsGivenTimesThenFails)
{
	utl::FailingAllocator failing(2);
	utl::MemoryAllocatorRef ref(failing.GetAPI());
	void* first = ref.Allocate(16);
	void* second = ref.Allocate(16);
	EXPECT_NE(first, nullptr);
	EXPECT_NE(second, nullptr);
	EXPECT_EQ(failing.Remaining(), 0u);
	EXPECT_EQ(failing.FailedCount(), 0u);

	EXPECT_EQ(ref.Allocate(16), nullptr);
	EXPECT_EQ(ref.Allocate(16), nullptr);
	EXPECT_EQ(failing.FailedCount(), 2u);

	// 開放は、失敗し始めた後でもできる
	EXPECT_TRUE(ref.Free(first));
	EXPECT_TRUE(ref.Free(second));
}

TEST(FailingAllocator, ZeroFailsFromTheStart)
{
	utl::FailingAllocator failing(0);
	utl::Vector<uint32_t> values(failing.GetAPI());
	EXPECT_FALSE(values.Resize(8));
	EXPECT_EQ(values.Size(), 0u);
	EXPECT_EQ(failing.FailedCount(), 1u);
}

TEST(FailingAllocator, SetRemainingCountsAgain)
{
	utl::FailingAllocator failing(0);
	utl::Vector<uint32_t> values(failing.GetAPI());
	EXPECT_FALSE(values.Reserve(4));
	failing.SetRemaining(1);
	EXPECT_TRUE(values.Reserve(4));
	EXPECT_EQ(failing.Remaining(), 0u);
	EXPECT_FALSE(values.Reserve(1024));
}

TEST(FailingAllocator, UsesInnerAllocator)
{
	utl::CountingAllocator counting;
	utl::FailingAllocator failing(1, counting.GetAPI());
	{
		utl::Vector<uint32_t> values(failing.GetAPI());
		EXPECT_TRUE(values.Reserve(4));
		// 失敗させた確保は、中のアロケータまで届かない
		EXPECT_FALSE(values.Reserve(1024));
	}
	const utl::AllocStats stats = counting.GetStats();
	EXPECT_EQ(stats.count, 1u);
	EXPECT_EQ(stats.freeCount, 1u);
}
