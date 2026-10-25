#include "ECOSTL/src/Container/Set/HashTable.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace
{
	class CountingAllocator
	{
	public:
		void* Allocate(const uint32_t size, const uint32_t align)
		{
			++allocCount;
			return utl::AlignedAllocate(size, align);
		}
		bool Free(void* memory)
		{
			++freeCount;
			utl::AlignedFree(memory);
			return true;
		}
		int allocCount = 0;
		int freeCount = 0;
	};
}

TEST(HashTable, DefaultIsEmpty)
{
	utl::HashTable table;
	EXPECT_TRUE(table.IsEmpty());
	EXPECT_EQ(table.Size(), 0u);
	EXPECT_EQ(table.FindId(123), utl::HashTable::INVALID_ID);
	EXPECT_FALSE(table.Contains(123));
}

TEST(HashTable, AssignsSequentialIdsFromZero)
{
	utl::HashTable table;
	EXPECT_EQ(table.GetOrAddId(900), 0u);
	EXPECT_EQ(table.GetOrAddId(5), 1u);
	EXPECT_EQ(table.GetOrAddId(UINT64_C(0xFFFFFFFF00000000)), 2u);
	EXPECT_EQ(table.Size(), 3u);
}

TEST(HashTable, SameHashReturnsSameId)
{
	utl::HashTable table;
	const uint32_t a = table.GetOrAddId(77);
	const uint32_t b = table.GetOrAddId(88);
	EXPECT_EQ(table.GetOrAddId(77), a);
	EXPECT_EQ(table.GetOrAddId(88), b);
	EXPECT_NE(a, b);
	EXPECT_EQ(table.Size(), 2u);
}

TEST(HashTable, FindDoesNotAssign)
{
	utl::HashTable table;
	table.GetOrAddId(1);
	EXPECT_EQ(table.FindId(2), utl::HashTable::INVALID_ID);
	EXPECT_EQ(table.Size(), 1u);
	EXPECT_EQ(table.FindId(1), 0u);
	EXPECT_TRUE(table.Contains(1));
}

TEST(HashTable, GetHashIsReverseLookup)
{
	utl::HashTable table;
	const uint32_t id = table.GetOrAddId(4242);
	EXPECT_EQ(table.GetHash(id), 4242u);
	EXPECT_EQ(table.GetHash(id + 1), utl::INVALID_HASH64);
}

TEST(HashTable, ManyHashesStayUniqueAndSequential)
{
	utl::HashTable table;
	for (uint32_t i = 0; i < 5000; ++i)
	{
		EXPECT_EQ(table.GetOrAddId(static_cast<utl::Hash64>(i) * 7919u + 13u), i);
	}
	for (uint32_t i = 0; i < 5000; ++i)
	{
		const utl::Hash64 hash = static_cast<utl::Hash64>(i) * 7919u + 13u;
		EXPECT_EQ(table.FindId(hash), i);
		EXPECT_EQ(table.GetHash(i), hash);
	}
	EXPECT_EQ(table.Size(), 5000u);
}

TEST(HashTable, ClearRestartsFromZero)
{
	utl::HashTable table;
	table.GetOrAddId(10);
	table.GetOrAddId(20);
	table.Clear();
	EXPECT_TRUE(table.IsEmpty());
	EXPECT_EQ(table.FindId(10), utl::HashTable::INVALID_ID);
	EXPECT_EQ(table.GetOrAddId(20), 0u);
}

TEST(HashTable, CopyIsIndependent)
{
	utl::HashTable a;
	a.GetOrAddId(1);
	utl::HashTable b(a);
	b.GetOrAddId(2);
	EXPECT_EQ(a.Size(), 1u);
	EXPECT_EQ(b.Size(), 2u);
	EXPECT_EQ(b.FindId(1), 0u);
}

TEST(HashTable, UsesGivenAllocatorAndFreesAll)
{
	CountingAllocator counter;
	utl::MemoryAllocatorDelegate delegate;
	delegate.Register<CountingAllocator, &CountingAllocator::Allocate, &CountingAllocator::Free>(&counter);
	{
		utl::HashTable table{ utl::MemoryAllocatorRef(&delegate) };
		for (uint32_t i = 0; i < 100; ++i)
		{
			table.GetOrAddId(i);
		}
		EXPECT_GT(counter.allocCount, 0);
	}
	EXPECT_EQ(counter.allocCount, counter.freeCount);
}

TEST(HashTable, SwapRemoveMovesLastIdIntoVacancy)
{
	utl::HashTable table;
	for (uint32_t i = 0; i < 10; ++i) table.GetOrAddId(100 + i);
	EXPECT_TRUE(table.SwapRemoveId(3));
	EXPECT_EQ(table.Size(), 9u);
	EXPECT_EQ(table.FindId(103), utl::HashTable::INVALID_ID);
	// 最後だった109が3番になる
	EXPECT_EQ(table.FindId(109), 3u);
	EXPECT_EQ(table.GetHash(3), 109u);
	for (uint32_t i = 0; i < 9; ++i)
	{
		EXPECT_EQ(table.FindId(table.GetHash(i)), i);
	}
	EXPECT_FALSE(table.SwapRemoveId(9));
	EXPECT_TRUE(table.SwapRemoveId(8));
	EXPECT_EQ(table.Size(), 8u);
}

TEST(HashTable, RemoveAndAddMatchesReference)
{
	utl::HashTable table;
	std::vector<utl::Hash64> ref;
	uint64_t x = 12345;
	for (int step = 0; step < 20000; ++step)
	{
		x = x * 6364136223846793005ULL + 1442695040888963407ULL;
		const utl::Hash64 h = (x >> 33) % 600;
		if ((x >> 20) % 3 == 0 && !ref.empty())
		{
			const uint32_t id = static_cast<uint32_t>((x >> 40) % ref.size());
			EXPECT_TRUE(table.SwapRemoveId(id));
			ref[id] = ref.back();
			ref.pop_back();
		}
		else if (table.FindId(h) == utl::HashTable::INVALID_ID)
		{
			EXPECT_EQ(table.GetOrAddId(h), ref.size());
			ref.push_back(h);
		}
	}
	ASSERT_EQ(table.Size(), ref.size());
	for (uint32_t i = 0; i < ref.size(); ++i)
	{
		EXPECT_EQ(table.GetHash(i), ref[i]);
		EXPECT_EQ(table.FindId(ref[i]), i);
	}
}

TEST(HashTable, AddIdAllowsSameHashAndMatchPicksOne)
{
	utl::HashTable table;
	const uint32_t a = table.AddId(5);
	const uint32_t b = table.AddId(5);
	EXPECT_NE(a, b);
	EXPECT_EQ(table.Size(), 2u);
	EXPECT_EQ(table.FindId(5, [&](const uint32_t id) { return id == b; }), b);
	EXPECT_EQ(table.FindId(5, [](const uint32_t) { return false; }), utl::HashTable::INVALID_ID);
}

TEST(HashTable, CopyAfterGrowthAndRemoval)
{
	utl::HashTable a;
	for (uint32_t i = 0; i < 1000; ++i) a.GetOrAddId(i * 31u);
	for (uint32_t i = 0; i < 900; ++i) a.SwapRemoveId(0);
	utl::HashTable b(a);
	ASSERT_EQ(b.Size(), a.Size());
	for (uint32_t i = 0; i < a.Size(); ++i)
	{
		EXPECT_EQ(b.FindId(a.GetHash(i)), i);
	}
	utl::HashTable c;
	c = utl::Move(b);
	EXPECT_EQ(c.Size(), a.Size());
	EXPECT_EQ(b.Size(), 0u);
}
