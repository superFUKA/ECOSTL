// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Container/Map/Hash/HashMap.h"

#include <gtest/gtest.h>

#include <string>
#include <unordered_map>

namespace
{
	/// <summary>
	/// 全てのキーを同じ位置に衝突させるハッシュ関数
	/// </summary>
	struct CollideHasher
	{
		utl::Hash64 operator()(const int&)const noexcept { return 0; }
	};

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

TEST(HashMapTest, DefaultIsEmpty) {
	utl::HashMap<int, int> map;
	EXPECT_EQ(map.Size(), 0u);
	EXPECT_TRUE(map.IsEmpty());
	EXPECT_EQ(map.Get(1), nullptr);
	EXPECT_FALSE(map.Remove(1));
	EXPECT_EQ(map.begin(), map.end());
}

TEST(HashMapTest, InsertGetOverwrite) {
	utl::HashMap<int, std::string> map;
	ASSERT_NE(map.Insert(5, 3, 'x'), nullptr);
	EXPECT_EQ(*map.Get(5), "xxx");
	map.Insert(5, "y");
	EXPECT_EQ(map.Size(), 1u);
	EXPECT_EQ(*map.Get(5), "y");
	EXPECT_TRUE(map.Contains(5));
	EXPECT_FALSE(map.Contains(6));
}

TEST(HashMapTest, GetOrAddAndIndex) {
	utl::HashMap<uint64_t, int> map;
	map[7] += 3;
	map[7] += 4;
	EXPECT_EQ(map.Size(), 1u);
	EXPECT_EQ(*map.Get(7), 7);
}

TEST(HashMapTest, ManyInsertRemoveMatchesReference) {
	utl::HashMap<int, int> map;
	std::unordered_map<int, int> ref;
	uint32_t r = 12345;
	for (int i = 0; i < 20000; ++i)
	{
		r = r * 1664525u + 1013904223u;
		const int key = static_cast<int>((r >> 8) % 512);
		if ((r & 3) == 0)
		{
			EXPECT_EQ(map.Remove(key), ref.erase(key) == 1);
		}
		else
		{
			map.Insert(key, i);
			ref[key] = i;
		}
		ASSERT_EQ(map.Size(), ref.size());
	}
	for (const auto& kv : ref)
	{
		ASSERT_NE(map.Get(kv.first), nullptr);
		EXPECT_EQ(*map.Get(kv.first), kv.second);
	}
	size_t count = 0;
	for (const auto& e : map)
	{
		EXPECT_EQ(ref.at(e.key), e.value);
		++count;
	}
	EXPECT_EQ(count, ref.size());
}

TEST(HashMapTest, CollisionsStillWork) {
	utl::HashMap<int, int, CollideHasher> map;
	for (int i = 0; i < 50; ++i) map.Insert(i, i * 2);
	for (int i = 0; i < 50; i += 2) EXPECT_TRUE(map.Remove(i));
	for (int i = 0; i < 50; ++i)
	{
		if (i % 2 == 0) EXPECT_EQ(map.Get(i), nullptr);
		else { ASSERT_NE(map.Get(i), nullptr); EXPECT_EQ(*map.Get(i), i * 2); }
	}
}

TEST(HashMapTest, ClearKeepsMemory) {
	utl::HashMap<int, std::string> map;
	for (int i = 0; i < 100; ++i) map.Insert(i, "v");
	const uint32_t cap = map.Capacity();
	map.Clear();
	EXPECT_EQ(map.Size(), 0u);
	EXPECT_EQ(map.Capacity(), cap);
	EXPECT_EQ(map.Get(3), nullptr);
	map.Insert(3, "a");
	EXPECT_EQ(*map.Get(3), "a");
}

TEST(HashMapTest, CopyAndMove) {
	utl::HashMap<int, std::string> a;
	for (int i = 0; i < 30; ++i) a.Insert(i, std::to_string(i));
	utl::HashMap<int, std::string> b(a);
	a.Remove(0);
	EXPECT_EQ(b.Size(), 30u);
	EXPECT_EQ(*b.Get(0), "0");
	utl::HashMap<int, std::string> c(utl::Move(b));
	EXPECT_EQ(b.Size(), 0u);
	EXPECT_EQ(c.Size(), 30u);
	utl::HashMap<int, std::string> d;
	d = c;
	d = utl::Move(c);
	EXPECT_EQ(d.Size(), 30u);
	EXPECT_EQ(*d.Get(29), "29");
}

TEST(HashMapTest, ReservePreventsRehash) {
	utl::HashMap<int, int> map(100);
	const uint32_t cap = map.Capacity();
	for (int i = 0; i < 100; ++i) map.Insert(i, i);
	EXPECT_EQ(map.Capacity(), cap);
}

TEST(HashMapTest, UsesAllocatorAndFreesAll) {
	CountingAllocator counter;
	utl::MemoryAllocatorDelegate delegate;
	delegate.Register<CountingAllocator, &CountingAllocator::Allocate, &CountingAllocator::Free>(&counter);
	{
		utl::HashMap<int, int> map{ utl::MemoryAllocatorRef(&delegate) };
		for (int i = 0; i < 100; ++i) map.Insert(i, i);
		EXPECT_GT(counter.allocCount, 0);
	}
	EXPECT_EQ(counter.allocCount, counter.freeCount);
}
