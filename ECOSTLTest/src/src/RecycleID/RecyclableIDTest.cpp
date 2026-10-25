#include "ECOSTL/src/RecycleID/RecyclableID.h"

#include <gtest/gtest.h>

#include <cstdint>

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
