// ecoSTL のコンテナの基本の試験（Vector / HashMap / SparseSet / BitSet）。
#include <gtest/gtest.h>

#include "ECOSTL.hpp"

// Vector の PushBack / Erase / Resize で要素数と中身が期待どおりになるか確かめる。
TEST(Vector, PushEraseResize)
{
	utl::Vector<int> v;
	for (int i = 0; i < 100; ++i) { v.PushBack(i); }
	ASSERT_EQ(v.Size(), 100u);
	EXPECT_EQ(v[0], 0);
	EXPECT_EQ(v[99], 99);
	EXPECT_TRUE(v.Erase(10));
	EXPECT_EQ(v.Size(), 99u);
	v.Insert(0, -5);
	EXPECT_EQ(v[0], -5);
	EXPECT_EQ(v[1], 0);
	v.Resize(10, 7);
	EXPECT_EQ(v.Size(), 10u);
	v.Clear();
	EXPECT_TRUE(v.IsEmpty());
}

// Vector のコピーが深いコピーで、元を変えてもコピー側が変わらないことを確かめる。
TEST(Vector, CopyIsDeep)
{
	utl::Vector<int> a;
	for (int i = 0; i < 5; ++i) { a.PushBack(i); }
	utl::Vector<int> b(a);
	b[0] = 99;
	EXPECT_EQ(a[0], 0);
	EXPECT_EQ(b.Size(), 5u);
}

// HashMap の Insert / Find / Remove が期待どおりに動くか確かめる。
TEST(HashMap, InsertFindRemove)
{
	utl::HashMap<uint32_t, int> m;
	for (uint32_t i = 0; i < 1000; ++i) { m.Insert(i * 7u, static_cast<int>(i)); }
	EXPECT_EQ(m.Size(), 1000u);
	for (uint32_t i = 0; i < 1000; ++i)
	{
		int* p = m.Find(i * 7u);
		ASSERT_NE(p, nullptr);
		EXPECT_EQ(*p, static_cast<int>(i));
	}
	EXPECT_EQ(m.Find(3u), nullptr);
	for (uint32_t i = 0; i < 1000; i += 2) { EXPECT_TRUE(m.Remove(i * 7u)); }
	EXPECT_FALSE(m.Remove(0u));
	EXPECT_EQ(m.Size(), 500u);
	for (uint32_t i = 1; i < 1000; i += 2) { ASSERT_NE(m.Find(i * 7u), nullptr); }
}

// SparseSet の Insert / Remove 後も dense 配列が詰まったままか確かめる。
TEST(SparseSet, InsertRemoveKeepsDense)
{
	utl::SparseSet set;
	for (utl::SparseKey k = 10; k < 20; ++k) { set.Insert(k); }
	EXPECT_EQ(set.Size(), 10u);
	EXPECT_TRUE(set.CheckActive(15));
	set.Remove(15);
	EXPECT_FALSE(set.CheckActive(15));
	EXPECT_EQ(set.Size(), 9u);
	for (utl::SparseKey k = 10; k < 20; ++k)
	{
		if (k == 15) { continue; }
		ASSERT_TRUE(set.CheckActive(k));
		EXPECT_EQ(set.GetKey(set.GetIndex(k)), k);
	}
}

// BitSet の Set / Reset / Test が各ビットを正しく切り替えるか確かめる。
TEST(BitSet, SetResetTest)
{
	utl::BitSet<100> b;
	b.Set(3); b.Set(64); b.Set(99);
	EXPECT_TRUE(b.Test(3));
	EXPECT_TRUE(b.Test(64));
	EXPECT_FALSE(b.Test(4));
	b.Reset(64);
	EXPECT_FALSE(b.Test(64));
	b.Clear();
	EXPECT_FALSE(b.Test(3));
}

// Vector のまとめて作る・置き換える・消す操作と、範囲 for が使えるか確かめる。
TEST(Vector, AssignEraseRangeAndRangeFor)
{
	utl::Vector<float> a;
	a.Assign(5u, 1.5f);
	ASSERT_EQ(a.Size(), 5u);
	float sum = 0.0f;
	for (const float x : a) { sum += x; }
	EXPECT_FLOAT_EQ(sum, 7.5f);

	utl::Vector<int> v({ 1, 2, 3 });
	v.PushBack(4);
	v.EmplaceBack(5);
	EXPECT_EQ(*v.Back(), 5);
	EXPECT_EQ(v[0], 1);
	v.Erase(1);
	EXPECT_EQ(v.Size(), 4u);
	EXPECT_EQ(v[1], 3);
	EXPECT_TRUE(v.IsValidIndex(3));
	EXPECT_FALSE(v.IsValidIndex(4));
	const int tail[] = { 8, 9 };
	for (const int x : tail) { v.PushBack(x); }
	EXPECT_EQ(*v.Back(), 9);   // 1 3 4 5 8 9
	ASSERT_TRUE(v.EraseRange(1, 3));   // 3 4 5 を一度で消す
	ASSERT_EQ(v.Size(), 3u);
	EXPECT_EQ(v[0], 1);
	EXPECT_EQ(v[1], 8);
	EXPECT_EQ(v[2], 9);
	EXPECT_FALSE(v.EraseRange(2, 2));   // 範囲外
	v.Resize(2);
	EXPECT_EQ(v.Size(), 2u);
	v.Assign(3u, 7);
	EXPECT_EQ(v[2], 7);

	utl::Vector<int> empty;
	utl::Vector<int> copy(empty);
	EXPECT_TRUE(copy.IsEmpty());
	utl::Vector<int> fromArray;
	fromArray.Assign(v.Begin(), v.Size());
	EXPECT_EQ(fromArray, v);
	fromArray.ShrinkTo(0);   // 要素数より小さくはならない
	EXPECT_EQ(fromArray.Size(), 3u);
}

// 容量いっぱいのときに、自分の要素を PushBack / EmplaceBack しても正しく写ることを確かめる（広げる前に古い領域を解放していた不具合の再発防止）。
TEST(Vector, PushBackOwnElementWhileGrowing)
{
	utl::Vector<utl::String> v;
	v.PushBack(utl::String("first-element-long-enough-to-use-heap"));
	while (v.Size() < 8) { v.PushBack(utl::String("x")); }
	v.ShrinkTo(v.Size());   // 容量を要素数ちょうどにして、次の追加で必ず広がるようにする
	ASSERT_NE(v.PushBack(v[0]), nullptr);
	EXPECT_EQ(*v.Back(), "first-element-long-enough-to-use-heap");
	v.ShrinkTo(v.Size());
	ASSERT_NE(v.EmplaceBack(v[0]), nullptr);
	EXPECT_EQ(*v.Back(), "first-element-long-enough-to-use-heap");
}

// String の連結・比較・検索などの基本操作が期待どおりか確かめる。
TEST(String, BasicOps)
{
	utl::String s;
	EXPECT_TRUE(s.IsEmpty());
	EXPECT_STREQ(s.CStr(), "");
	s = "abc";
	s += "def";
	s += 'g';
	EXPECT_EQ(s, "abcdefg");
	EXPECT_EQ(s.Size(), 7u);
	EXPECT_EQ(s.Find('d'), 3u);
	EXPECT_EQ(s.Find('z'), utl::String::NOT_FOUND);
	EXPECT_EQ(s.Find("efg"), 4u);
	EXPECT_EQ(s.Substr(2, 3), "cde");
	EXPECT_EQ(s.Substr(5), "fg");
	EXPECT_EQ(s.FindLast('g'), 6u);
	EXPECT_EQ(s.Back(), 'g');
	s += s;   // 自分自身の追加
	EXPECT_EQ(s, "abcdefgabcdefg");
	utl::String t = "x" + utl::ToString(42) + '_' + utl::String("y");
	EXPECT_EQ(t, "x42_y");
	utl::String moved(static_cast<utl::String&&>(t));
	EXPECT_EQ(moved, "x42_y");
	EXPECT_TRUE(t.IsEmpty());
	EXPECT_TRUE(utl::String("a") < utl::String("b"));
	EXPECT_EQ(utl::ToString(7u), "7");
	EXPECT_EQ(utl::ToString(-3), "-3");
}

// ChunkVector が増えても要素のアドレスが変わらず、範囲 for も使えることを確かめる。
TEST(ChunkVector, StableAddressAndRangeFor)
{
	utl::ChunkVector<int> c(4);
	int* first = c.EmplaceBack(10);
	for (int i = 1; i < 100; ++i) { c.EmplaceBack(i * 10); }
	EXPECT_EQ(first, &c[0]);   // 追加してもアドレスは変わらない
	EXPECT_EQ(c.Size(), 100u);
	EXPECT_EQ(*c.Back(), 990);
	int sum = 0;
	for (const int v : c) { sum += v; }
	EXPECT_EQ(sum, 10 + 10 * 4950);   // 先頭の 10 + 10*(1+..+99)
	EXPECT_FALSE(c.IsEmpty());
}

namespace
{
	// 確保・開放のフックで受け取った値を数える
	struct HookCounter
	{
		uint32_t bytes = 0;
		uint32_t allocCalls = 0;
		uint32_t freeCalls = 0;
	};
	void OnAllocate(void* user, void*, const uint32_t size, uint32_t) { HookCounter* c = static_cast<HookCounter*>(user); c->bytes += size; ++c->allocCalls; }
	void OnFree(void* user, void*) { ++static_cast<HookCounter*>(user)->freeCalls; }
}

// Vector が伸びたときの確保が、渡した CountingAllocator に数えられることを確かめる。
TEST(CountingAllocator, VectorGrowthIsCounted)
{
	utl::CountingAllocator counting;
	{
		utl::Vector<int> v(counting.GetAPI());
		for (int i = 0; i < 100; ++i) { v.PushBack(i); }
		EXPECT_GT(counting.GetStats().count, 0u);
		EXPECT_GT(counting.GetStats().bytes, 0u);

		// 事前確保すれば、PushBack では増えない
		utl::Vector<int> w(counting.GetAPI());
		w.Reserve(100);
		const uint64_t reserved = counting.GetStats().count;
		for (int i = 0; i < 100; ++i) { w.PushBack(i); }
		EXPECT_EQ(counting.GetStats().count, reserved);
	}
	// 破棄で全て返される
	EXPECT_EQ(counting.GetStats().count, counting.GetStats().freeCount);
}

// 確保・開放のフックが呼ばれ、外した後は呼ばれなくなることを確かめる。
TEST(CountingAllocator, HookIsCalledAndCanBeRemoved)
{
	HookCounter counter;
	utl::CountingAllocator counting;
	utl::AllocHook hook;
	hook.onAllocate = &OnAllocate;
	hook.onFree = &OnFree;
	hook.user = &counter;
	counting.SetHook(hook);
	utl::MemoryAllocatorRef holder(counting.GetAPI());
	void* p = holder.Allocate(64);
	holder.Free(p);
	counting.SetHook(utl::AllocHook());
	EXPECT_EQ(counter.allocCalls, 1u);
	EXPECT_EQ(counter.freeCalls, 1u);
	EXPECT_EQ(counter.bytes, 64u);
	void* q = holder.Allocate(8);
	holder.Free(q);
	EXPECT_EQ(counter.allocCalls, 1u);
}

// String の確保も、渡したアロケータを通ることを確かめる。
TEST(CountingAllocator, StringUsesGivenAllocator)
{
	utl::CountingAllocator counting;
	{
		utl::String s(counting.GetAPI());
		s += "hello, allocator";
		EXPECT_EQ(s, "hello, allocator");
		EXPECT_GT(counting.GetStats().count, 0u);
		utl::String copy(s);
		EXPECT_EQ(copy, s);
	}
	EXPECT_EQ(counting.GetStats().count, counting.GetStats().freeCount);
}
