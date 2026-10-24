// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Container/Map/DenceMap.h"

#include <gtest/gtest.h>

#include <string>


namespace
{
	/// <summary>
	/// 確保と開放の回数を数えるメモリアロケータ
	/// </summary>
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

TEST(DenceMapTest, DefaultIsEmpty) {
	utl::DenceMap<int> map;
	EXPECT_EQ(map.Size(), 0u);
	EXPECT_FALSE(map.CheckActive(0));
	EXPECT_EQ(map.Get(0), nullptr);
}

TEST(DenceMapTest, InsertAndGet) {
	utl::DenceMap<int> map;
	int* p = map.Insert(5, 10);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(*p, 10);
	EXPECT_EQ(map.Size(), 1u);
	EXPECT_TRUE(map.CheckActive(5));
	ASSERT_NE(map.Get(5), nullptr);
	EXPECT_EQ(*map.Get(5), 10);
	EXPECT_EQ(map.Get(4), nullptr);
}

TEST(DenceMapTest, InsertLvalue) {
	utl::DenceMap<std::string> map;
	std::string s = "abc";
	map.Insert(1, s);
	EXPECT_EQ(*map.Get(1), "abc");
	EXPECT_EQ(s, "abc");
}

TEST(DenceMapTest, InsertConstructorArgs) {
	utl::DenceMap<std::string> map;
	map.Insert(2, 3, 'x');
	EXPECT_EQ(*map.Get(2), "xxx");
}

TEST(DenceMapTest, InsertExistingKeyOverwrites) {
	utl::DenceMap<int> map;
	map.Insert(3, 1);
	int* p = map.Insert(3, 2);
	EXPECT_EQ(map.Size(), 1u);
	EXPECT_EQ(*p, 2);
	EXPECT_EQ(*map.Get(3), 2);
}

TEST(DenceMapTest, SparseKeys) {
	utl::DenceMap<int> map;
	map.Insert(1000, 1);
	map.Insert(0, 2);
	map.Insert(37, 3);
	EXPECT_EQ(map.Size(), 3u);
	EXPECT_EQ(*map.Get(1000), 1);
	EXPECT_EQ(*map.Get(0), 2);
	EXPECT_EQ(*map.Get(37), 3);
}

TEST(DenceMapTest, ValuesArePackedInInsertOrder) {
	utl::DenceMap<int> map;
	map.Insert(50, 1);
	map.Insert(10, 2);
	map.Insert(30, 3);
	const int* begin = map.Begin();
	EXPECT_EQ(begin[0], 1);
	EXPECT_EQ(begin[1], 2);
	EXPECT_EQ(begin[2], 3);
	EXPECT_EQ(map.GetKeys()[0], 50u);
	EXPECT_EQ(map.GetKeys()[1], 10u);
	EXPECT_EQ(map.GetKeys()[2], 30u);
}

TEST(DenceMapTest, RemoveMiddleMovesBackElement) {
	utl::DenceMap<int> map;
	map.Insert(1, 10);
	map.Insert(2, 20);
	map.Insert(3, 30);
	map.Remove(1);
	EXPECT_EQ(map.Size(), 2u);
	EXPECT_FALSE(map.CheckActive(1));
	EXPECT_EQ(map.Get(1), nullptr);
	EXPECT_EQ(*map.Get(2), 20);
	EXPECT_EQ(*map.Get(3), 30);
	// 末尾の要素が削除した場所に移る
	EXPECT_EQ(map.Begin()[0], 30);
	EXPECT_EQ(map.Begin()[1], 20);
}

TEST(DenceMapTest, RemoveBack) {
	utl::DenceMap<int> map;
	map.Insert(1, 10);
	map.Insert(2, 20);
	map.Remove(2);
	EXPECT_EQ(map.Size(), 1u);
	EXPECT_EQ(*map.Get(1), 10);
	EXPECT_FALSE(map.CheckActive(2));
}

TEST(DenceMapTest, RemoveOnlyElement) {
	utl::DenceMap<int> map;
	map.Insert(7, 1);
	map.Remove(7);
	EXPECT_EQ(map.Size(), 0u);
	EXPECT_FALSE(map.CheckActive(7));
}

TEST(DenceMapTest, RemoveInactiveKeyDoesNothing) {
	utl::DenceMap<int> map;
	map.Insert(1, 10);
	map.Remove(2);
	map.Remove(9999);
	EXPECT_EQ(map.Size(), 1u);
	EXPECT_EQ(*map.Get(1), 10);
}

TEST(DenceMapTest, ReinsertAfterRemove) {
	utl::DenceMap<int> map;
	map.Insert(1, 10);
	map.Insert(2, 20);
	map.Remove(1);
	map.Insert(1, 11);
	EXPECT_EQ(map.Size(), 2u);
	EXPECT_EQ(*map.Get(1), 11);
	EXPECT_EQ(*map.Get(2), 20);
}

TEST(DenceMapTest, GetOrAddAddsDefault) {
	utl::DenceMap<int> map;
	int& v = map.GetOrAdd(4);
	EXPECT_EQ(v, 0);
	EXPECT_EQ(map.Size(), 1u);
	v = 9;
	EXPECT_EQ(map.GetOrAdd(4), 9);
	EXPECT_EQ(map.Size(), 1u);
}

TEST(DenceMapTest, SubscriptAddsAndAssigns) {
	utl::DenceMap<int> map;
	map[8] = 5;
	EXPECT_EQ(map.Size(), 1u);
	EXPECT_EQ(*map.Get(8), 5);
	map[8] = 6;
	EXPECT_EQ(map.Size(), 1u);
	EXPECT_EQ(*map.Get(8), 6);
}

TEST(DenceMapTest, ConstGet) {
	utl::DenceMap<int> src;
	src.Insert(1, 10);
	const utl::DenceMap<int>& map = src;
	ASSERT_NE(map.Get(1), nullptr);
	EXPECT_EQ(*map.Get(1), 10);
	EXPECT_EQ(map.Get(2), nullptr);
	EXPECT_EQ(map.Begin()[0], 10);
}

TEST(DenceMapTest, ClearRemovesAllAndCanReuse) {
	utl::DenceMap<int> map;
	map.Insert(1, 10);
	map.Insert(2, 20);
	map.Clear();
	EXPECT_EQ(map.Size(), 0u);
	EXPECT_FALSE(map.CheckActive(1));
	EXPECT_FALSE(map.CheckActive(2));
	map.Insert(2, 21);
	EXPECT_EQ(map.Size(), 1u);
	EXPECT_EQ(*map.Get(2), 21);
}

TEST(DenceMapTest, Fit) {
	utl::DenceMap<int> map(16);
	map.Insert(1, 10);
	map.Insert(2, 20);
	map.Fit();
	EXPECT_EQ(map.Size(), 2u);
	EXPECT_EQ(*map.Get(1), 10);
	EXPECT_EQ(*map.Get(2), 20);
}

TEST(DenceMapTest, CopyIsIndependent) {
	utl::DenceMap<int> a;
	a.Insert(1, 10);
	a.Insert(2, 20);
	utl::DenceMap<int> b(a);
	b.Insert(1, 99);
	b.Remove(2);
	EXPECT_EQ(*a.Get(1), 10);
	EXPECT_EQ(*a.Get(2), 20);
	EXPECT_EQ(a.Size(), 2u);
	EXPECT_EQ(*b.Get(1), 99);
	EXPECT_EQ(b.Size(), 1u);
}

TEST(DenceMapTest, MoveConstruct) {
	utl::DenceMap<std::string> a;
	a.Insert(1, "one");
	a.Insert(2, "two");
	utl::DenceMap<std::string> b(utl::Move(a));
	EXPECT_EQ(b.Size(), 2u);
	EXPECT_EQ(*b.Get(1), "one");
	EXPECT_EQ(*b.Get(2), "two");
	EXPECT_EQ(a.Size(), 0u);
}

TEST(DenceMapTest, MoveAssign) {
	utl::DenceMap<int> a;
	a.Insert(1, 10);
	utl::DenceMap<int> b;
	b.Insert(5, 50);
	b = utl::Move(a);
	EXPECT_EQ(b.Size(), 1u);
	EXPECT_EQ(*b.Get(1), 10);
	EXPECT_FALSE(b.CheckActive(5));
}

TEST(DenceMapTest, ManyElementsInsertAndRemove) {
	utl::DenceMap<int> map;
	for (uint32_t i = 0; i < 200; ++i)
	{
		map.Insert(i * 3, static_cast<int>(i));
	}
	for (uint32_t i = 0; i < 200; i += 2)
	{
		map.Remove(i * 3);
	}
	EXPECT_EQ(map.Size(), 100u);
	for (uint32_t i = 0; i < 200; ++i)
	{
		if (i % 2 == 0)
		{
			EXPECT_FALSE(map.CheckActive(i * 3));
		}
		else
		{
			ASSERT_NE(map.Get(i * 3), nullptr);
			EXPECT_EQ(*map.Get(i * 3), static_cast<int>(i));
		}
	}
}

TEST(DenceMapTest, UsesGivenAllocators) {
	CountingAllocator utlAlloc;
	CountingAllocator valueAlloc;
	utl::MemoryAllocatorDelegate utlDelegate;
	utl::MemoryAllocatorDelegate valueDelegate;
	utlDelegate.Register<CountingAllocator, &CountingAllocator::Allocate, &CountingAllocator::Free>(&utlAlloc);
	valueDelegate.Register<CountingAllocator, &CountingAllocator::Allocate, &CountingAllocator::Free>(&valueAlloc);
	{
		utl::DenceMap<int> map(4, utl::MemoryAllocatorRef(&utlDelegate), utl::MemoryAllocatorRef(&valueDelegate));
		map.Insert(1, 1);
		map.Insert(2, 2);
		EXPECT_GT(valueAlloc.allocCount, 0);
		EXPECT_GT(utlAlloc.allocCount, 0);
	}
	EXPECT_EQ(utlAlloc.allocCount, utlAlloc.freeCount);
	EXPECT_EQ(valueAlloc.allocCount, valueAlloc.freeCount);
}
