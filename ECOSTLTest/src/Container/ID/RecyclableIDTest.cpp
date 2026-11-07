#include "ECOSTL/Container/ID/RecyclableID.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>

namespace
{
	// fail が true の間は確保に失敗するアロケータ
	class FailableAllocator
	{
	public:
		FailableAllocator()
		{
			api.Register<FailableAllocator, &FailableAllocator::Allocate, &FailableAllocator::Free>(this);
		}
		void* Allocate(uint32_t size, uint32_t)
		{
			return fail ? nullptr : malloc(size);
		}
		bool Free(void* memory)
		{
			free(memory);
			return true;
		}

		bool fail = false;
		utl::MemoryAllocatorDelegate api;
	};
}

TEST(RecyclableID, AllocatesSequentialIndices)
{
	utl::RecyclableIDAllocator alloc;
	const utl::RecyclableID a = alloc.Allocate();
	const utl::RecyclableID b = alloc.Allocate();
	EXPECT_EQ(a.index, 0u);
	EXPECT_EQ(b.index, 1u);
	EXPECT_EQ(a.version, 1u);
	EXPECT_TRUE(alloc.CheckActive(a));
	EXPECT_TRUE(alloc.CheckActive(b));
}

TEST(RecyclableID, DeallocatedIsInactive)
{
	utl::RecyclableIDAllocator alloc;
	const utl::RecyclableID a = alloc.Allocate();
	alloc.Deallocate(a);
	EXPECT_FALSE(alloc.CheckActive(a));
}

TEST(RecyclableID, ReusesIndexWithNewVersion)
{
	utl::RecyclableIDAllocator alloc;
	const utl::RecyclableID a = alloc.Allocate();
	alloc.Deallocate(a);
	const utl::RecyclableID b = alloc.Allocate();
	EXPECT_EQ(b.index, a.index);
	EXPECT_EQ(b.version, a.version + 1);
	EXPECT_FALSE(alloc.CheckActive(a));
	EXPECT_TRUE(alloc.CheckActive(b));
}

TEST(RecyclableID, DoubleDeallocateDoesNotDuplicateIndex)
{
	utl::RecyclableIDAllocator alloc;
	const utl::RecyclableID a = alloc.Allocate();
	alloc.Deallocate(a);
	alloc.Deallocate(a);
	const utl::RecyclableID b = alloc.Allocate();
	const utl::RecyclableID c = alloc.Allocate();
	EXPECT_NE(b.index, c.index);
}

TEST(RecyclableID, UnknownIdIsInactive)
{
	utl::RecyclableIDAllocator alloc;
	EXPECT_FALSE(alloc.CheckActive(5, 1));
	alloc.Deallocate(5, 1);
	EXPECT_EQ(alloc.Allocate().index, 0u);
}

namespace
{
	struct CustomID
	{
		uint16_t idx = 0;
		uint8_t ver = 0;
	};
}

TEST(RecyclableID, AllocatesIntoCustomFields)
{
	utl::RecyclableIDAllocator alloc;
	CustomID id;
	alloc.Allocate(id, &CustomID::idx, &CustomID::ver);
	alloc.Allocate(id, &CustomID::idx, &CustomID::ver);
	EXPECT_EQ(id.idx, 1u);
	EXPECT_EQ(id.ver, 1u);
	EXPECT_TRUE(alloc.CheckActive(id.idx, id.ver));
}

// 新しい添え字の表を広げられなければ、無効な ID（version 0）を返し、状態は変わらない
TEST(RecyclableID, AllocateFailureReturnsInvalid)
{
	FailableAllocator failable;
	utl::RecyclableIDAllocator alloc(&failable.api);
	failable.fail = true;

	const utl::RecyclableID failed = alloc.Allocate();
	EXPECT_EQ(failed.version, 0u);
	EXPECT_FALSE(alloc.CheckActive(failed));

	// 失敗した分の添え字は飛ばさない
	failable.fail = false;
	const utl::RecyclableID ok = alloc.Allocate();
	EXPECT_EQ(ok.index, 0u);
	EXPECT_EQ(ok.version, 1u);
}

// まとめて生成するときに表を広げられなければ、1つも生成しない（リサイクル待ちも使わない）
TEST(RecyclableID, AllocateManyFailureAllocatesNothing)
{
	FailableAllocator failable;
	utl::RecyclableIDAllocator alloc(&failable.api);
	const utl::RecyclableID a = alloc.Allocate();
	ASSERT_TRUE(alloc.Deallocate(a));
	failable.fail = true;

	uint32_t called = 0;
	EXPECT_FALSE(alloc.AllocateMany(5, [&](uint32_t, uint32_t) { ++called; }));
	EXPECT_EQ(called, 0u);
	EXPECT_EQ(alloc.GetVersion(1), 0u);

	// リサイクル待ちだけで足りるなら、確保なしで成功する
	EXPECT_TRUE(alloc.AllocateMany(1, [&](uint32_t index, uint32_t version)
		{
			EXPECT_EQ(index, 0u);
			EXPECT_EQ(version, 2u);
			++called;
		}));
	EXPECT_EQ(called, 1u);
}

// リサイクル待ちを広げられなければ、開放せずに false を返し、ID は有効なまま
TEST(RecyclableID, DeallocateFailureKeepsActive)
{
	FailableAllocator failable;
	utl::RecyclableIDAllocator alloc(&failable.api);
	const utl::RecyclableID a = alloc.Allocate();
	failable.fail = true;

	EXPECT_FALSE(alloc.Deallocate(a));
	EXPECT_TRUE(alloc.CheckActive(a));

	failable.fail = false;
	EXPECT_TRUE(alloc.Deallocate(a));
	EXPECT_FALSE(alloc.CheckActive(a));
	// 無効な ID の開放は false
	EXPECT_FALSE(alloc.Deallocate(a));
}

// 事前確保の失敗は false で分かる
TEST(RecyclableID, ReserveFailureReturnsFalse)
{
	FailableAllocator failable;
	utl::RecyclableIDAllocator alloc(&failable.api);
	failable.fail = true;
	EXPECT_FALSE(alloc.ReserveIndices(10));
	EXPECT_FALSE(alloc.ReserveRecycle(10));

	failable.fail = false;
	EXPECT_TRUE(alloc.ReserveIndices(10));
	EXPECT_TRUE(alloc.ReserveRecycle(10));
}
