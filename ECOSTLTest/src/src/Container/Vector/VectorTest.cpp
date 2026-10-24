// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Container/Vector/Vector.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <set>
#include <string>


namespace
{
	/// <summary>
	/// 生成と破棄の回数を数えるテスト用の型
	/// </summary>
	struct Tracked
	{
		static inline int alive = 0;
		static inline int copies = 0;
		static inline int moves = 0;

		static void ResetCount()
		{
			alive = 0;
			copies = 0;
			moves = 0;
		}

		Tracked(int v = 0)
			: value(v)
		{
			++alive;
		}
		Tracked(const Tracked& src)
			: value(src.value)
		{
			++alive;
			++copies;
		}
		Tracked(Tracked&& src)noexcept
			: value(src.value)
		{
			src.value = -1;
			++alive;
			++moves;
		}
		Tracked& operator=(const Tracked&) = default;
		~Tracked()
		{
			--alive;
		}

		int value;
	};

	/// <summary>
	/// ムーブだけできる型
	/// </summary>
	struct MoveOnly
	{
		MoveOnly(int v = 0)
			: value(v)
		{}
		MoveOnly(const MoveOnly&) = delete;
		MoveOnly(MoveOnly&& src)noexcept
			: value(src.value)
		{
			src.value = -1;
		}

		int value;
	};

	/// <summary>
	/// 確保と開放の回数を数えるテスト用のアロケータ
	/// </summary>
	class CountingAllocator
	{
	public:
		void* Allocate(uint32_t size, uint32_t align)
		{
			(void)align;
			++allocCount;
			void* mem = malloc(size);
			live.insert(mem);
			return mem;
		}
		bool Free(void* memory)
		{
			++freeCount;
			if (live.erase(memory) == 0)
			{
				++badFreeCount;
				return false;
			}
			free(memory);
			return true;
		}

		int allocCount = 0;
		int freeCount = 0;
		int badFreeCount = 0;
		std::set<void*> live;
	};

	class VectorTest : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			Tracked::ResetCount();
			delegate.Register<CountingAllocator, &CountingAllocator::Allocate, &CountingAllocator::Free>(&alloc);
		}
		void TearDown() override
		{
			EXPECT_EQ(Tracked::alive, 0);
			EXPECT_EQ(alloc.badFreeCount, 0);
			EXPECT_TRUE(alloc.live.empty());
		}

		CountingAllocator alloc;
		utl::MemoryAllocatorDelegate delegate;
	};

	template<typename Type>
	std::string ToString(const utl::Vector<Type>& v)
	{
		std::string s;
		for (const Type& e : v)
		{
			s += std::to_string(e.value) + ",";
		}
		return s;
	}
	std::string ToString(const utl::Vector<int>& v)
	{
		std::string s;
		for (int e : v)
		{
			s += std::to_string(e) + ",";
		}
		return s;
	}
}

//----------------------------------------
// 作成
//----------------------------------------

TEST_F(VectorTest, DefaultIsEmpty) {
	utl::Vector<int> v;

	EXPECT_TRUE(v.IsEmpty());
	EXPECT_EQ(v.Size(), 0u);
	EXPECT_EQ(v.Capacity(), 0u);
	EXPECT_EQ(v.Begin(), nullptr);
	EXPECT_EQ(v.Back(), nullptr);
	EXPECT_EQ(v.MemAllocator().Get(), nullptr);
}

TEST_F(VectorTest, CapacityConstructorAllocates) {
	utl::Vector<int> v(10, &delegate);

	EXPECT_EQ(v.Capacity(), 10u);
	EXPECT_NE(v.Begin(), nullptr);
	EXPECT_EQ(alloc.allocCount, 1);

	v.PushBack(1);
	EXPECT_EQ(v[0], 1);
}

TEST_F(VectorTest, UsesGivenAllocator) {
	{
		utl::Vector<Tracked> v(&delegate);
		for (int i = 0; i < 20; ++i)
		{
			v.PushBack(Tracked(i));
		}
		EXPECT_EQ(v.MemAllocator().Get(), &delegate);
		EXPECT_GT(alloc.allocCount, 0);
	}
	EXPECT_EQ(alloc.allocCount, alloc.freeCount);
}

//----------------------------------------
// 代入
//----------------------------------------

TEST_F(VectorTest, CopyConstruct) {
	utl::Vector<Tracked> src(&delegate);
	src.EmplaceBack(1);
	src.EmplaceBack(2);

	utl::Vector<Tracked> dst(src);

	EXPECT_EQ(ToString(dst), "1,2,");
	EXPECT_EQ(ToString(src), "1,2,");
	EXPECT_NE(dst.Begin(), src.Begin());
	EXPECT_EQ(dst.MemAllocator().Get(), &delegate);
}

TEST_F(VectorTest, CopyAssignEmpty) {
	utl::Vector<int> src(&delegate);
	utl::Vector<int> dst(&delegate);
	dst.PushBack(1);

	const uint32_t capacity = dst.Capacity();

	dst = src;

	// メモリは使い回すので残る
	EXPECT_TRUE(dst.IsEmpty());
	EXPECT_EQ(dst.Capacity(), capacity);
}

TEST_F(VectorTest, CopyAssignSelf) {
	utl::Vector<Tracked> v(&delegate);
	v.EmplaceBack(1);
	utl::Vector<Tracked>& ref = v;

	v = ref;

	EXPECT_EQ(ToString(v), "1,");
}

TEST_F(VectorTest, MoveConstructAndAssign) {
	utl::Vector<Tracked> src(&delegate);
	src.EmplaceBack(1);
	Tracked* begin = src.Begin();

	utl::Vector<Tracked> dst(utl::Move(src));
	EXPECT_EQ(dst.Begin(), begin);
	EXPECT_TRUE(src.IsEmpty());
	EXPECT_EQ(src.Begin(), nullptr);

	utl::Vector<Tracked> other;
	other.EmplaceBack(5);
	other = utl::Move(dst);
	EXPECT_EQ(ToString(other), "1,");
	EXPECT_EQ(other.MemAllocator().Get(), &delegate);
	EXPECT_EQ(Tracked::alive, 1);
}

//----------------------------------------
// 追加
//----------------------------------------

TEST_F(VectorTest, PushBackGrows) {
	utl::Vector<int> v(&delegate);
	for (int i = 0; i < 100; ++i)
	{
		v.PushBack(i);
	}

	ASSERT_EQ(v.Size(), 100u);
	EXPECT_GE(v.Capacity(), 100u);
	for (uint32_t i = 0; i < 100; ++i)
	{
		EXPECT_EQ(v[i], static_cast<int>(i));
	}
	EXPECT_EQ(*v.Back(), 99);
}

TEST_F(VectorTest, PushBackOwnElementWhileGrowing) {
	utl::Vector<Tracked> v(&delegate);
	v.EmplaceBack(7);
	// 容量が足りない状態にする
	while (v.Size() < v.Capacity())
	{
		v.EmplaceBack(0);
	}

	v.PushBack(v[0]);

	EXPECT_EQ(v.Back()->value, 7);
}

TEST_F(VectorTest, PushBackOwnElementWhileGrowingTrivial) {
	utl::Vector<int> v(&delegate);
	v.PushBack(7);
	while (v.Size() < v.Capacity())
	{
		v.PushBack(0);
	}

	v.PushBack(v[0]);

	EXPECT_EQ(*v.Back(), 7);
}

TEST_F(VectorTest, MoveOnlyType) {
	utl::Vector<MoveOnly> v(&delegate);
	for (int i = 0; i < 10; ++i)
	{
		v.EmplaceBack(i);
	}
	v.Insert(0, MoveOnly(100));
	v.Erase(1);

	EXPECT_EQ(v.Size(), 10u);
	EXPECT_EQ(v[0].value, 100);
	EXPECT_EQ(v[1].value, 1);
	EXPECT_EQ(v[9].value, 9);
}

TEST_F(VectorTest, AppendRange) {
	utl::Vector<Tracked> a(&delegate);
	a.EmplaceBack(1);
	utl::Vector<Tracked> b(&delegate);
	b.EmplaceBack(2);
	b.EmplaceBack(3);

	a.AppendRange(b);

	EXPECT_EQ(ToString(a), "1,2,3,");
}

TEST_F(VectorTest, AppendRangeSelf) {
	utl::Vector<Tracked> v(&delegate);
	v.EmplaceBack(1);
	v.EmplaceBack(2);

	v.AppendRange(v);

	EXPECT_EQ(ToString(v), "1,2,1,2,");
}

//----------------------------------------
// 挿入
//----------------------------------------

TEST_F(VectorTest, InsertMiddleAndEnd) {
	utl::Vector<Tracked> v(&delegate);
	v.EmplaceBack(1);
	v.EmplaceBack(3);

	EXPECT_EQ(v.Insert(1, Tracked(2))->value, 2);
	EXPECT_EQ(v.Insert(3, Tracked(4))->value, 4);
	EXPECT_EQ(v.Emplace(0, 0)->value, 0);

	EXPECT_EQ(ToString(v), "0,1,2,3,4,");
}

TEST_F(VectorTest, InsertOwnElement) {
	utl::Vector<Tracked> v(&delegate);
	v.EmplaceBack(1);
	v.EmplaceBack(2);
	v.Fit();

	// 容量が足りない状態でずらされる要素を渡す
	v.Insert(0, v[1]);

	EXPECT_EQ(ToString(v), "2,1,2,");
}

TEST_F(VectorTest, InsertOwnElementTrivial) {
	utl::Vector<int> v(&delegate);
	v.PushBack(1);
	v.PushBack(2);
	v.Reserve(10);

	v.Insert(0, v[0]);

	EXPECT_EQ(ToString(v), "1,1,2,");
}

//----------------------------------------
// 削除
//----------------------------------------

TEST_F(VectorTest, EraseKeepsOrder) {
	utl::Vector<Tracked> v(&delegate);
	for (int i = 0; i < 5; ++i)
	{
		v.EmplaceBack(i);
	}

	EXPECT_TRUE(v.Erase(1));
	EXPECT_TRUE(v.Erase(3));
	EXPECT_FALSE(v.Erase(3));

	EXPECT_EQ(ToString(v), "0,2,3,");
	EXPECT_EQ(Tracked::alive, 3);
}

TEST_F(VectorTest, PopBackAndClear) {
	utl::Vector<Tracked> v(&delegate);
	v.EmplaceBack(1);
	v.EmplaceBack(2);

	v.PopBack();
	EXPECT_EQ(ToString(v), "1,");

	const uint32_t capacity = v.Capacity();
	v.Clear();
	EXPECT_TRUE(v.IsEmpty());
	EXPECT_EQ(v.Capacity(), capacity);
	EXPECT_EQ(Tracked::alive, 0);

	v.PopBack();
	EXPECT_TRUE(v.IsEmpty());
}

TEST_F(VectorTest, ClearKeepsMemory) {
	utl::Vector<int> v(&delegate);
	v.PushBack(1);
	const int* begin = v.Begin();
	const uint32_t capacity = v.Capacity();

	v.Clear();
	v.PushBack(2);

	EXPECT_EQ(v.Begin(), begin);
	EXPECT_EQ(v.Capacity(), capacity);
	EXPECT_EQ(alloc.allocCount, 1);
	EXPECT_EQ(alloc.freeCount, 0);
}

TEST_F(VectorTest, ResetFreesMemory) {
	utl::Vector<Tracked> v(&delegate);
	v.EmplaceBack(1);

	v.Reset();

	EXPECT_TRUE(v.IsEmpty());
	EXPECT_EQ(Tracked::alive, 0);

	EXPECT_EQ(v.Capacity(), 0u);
	EXPECT_EQ(v.Begin(), nullptr);
	EXPECT_EQ(alloc.allocCount, alloc.freeCount);

	// 開放した後もそのまま使える
	v.EmplaceBack(2);
	EXPECT_EQ(ToString(v), "2,");
}

//----------------------------------------
// 取得
//----------------------------------------

TEST_F(VectorTest, GetOutOfRangeIsNull) {
	utl::Vector<int> v(&delegate);
	v.PushBack(1);
	const utl::Vector<int>& cv = v;

	EXPECT_EQ(*v.Get(0), 1);
	EXPECT_EQ(v.Get(1), nullptr);
	EXPECT_EQ(*cv.Get(0), 1);
	EXPECT_TRUE(v.IsValidIndex(0));
	EXPECT_FALSE(v.IsValidIndex(1));
	EXPECT_EQ(cv.End() - cv.Begin(), 1);
}

//----------------------------------------
// メモリ操作
//----------------------------------------

TEST_F(VectorTest, GrowsByOneAndHalf) {
	utl::Vector<int> v(&delegate);
	uint32_t capacities[6] = {};
	uint32_t count = 0;
	for (int i = 0; i < 9; ++i)
	{
		const uint32_t before = v.Capacity();
		v.PushBack(i);
		if (v.Capacity() != before)
		{
			capacities[count++] = v.Capacity();
		}
	}

	// 最初は2、そのあとは1.5倍ずつ
	ASSERT_EQ(count, 5u);
	EXPECT_EQ(capacities[0], 2u);
	EXPECT_EQ(capacities[1], 3u);
	EXPECT_EQ(capacities[2], 4u);
	EXPECT_EQ(capacities[3], 6u);
	EXPECT_EQ(capacities[4], 9u);
}

TEST_F(VectorTest, ResizeGrowAndShrink) {
	utl::Vector<Tracked> v(&delegate);

	v.Resize(3, 5);
	EXPECT_EQ(ToString(v), "5,5,5,");

	v.Resize(1);
	EXPECT_EQ(ToString(v), "5,");
	EXPECT_EQ(Tracked::alive, 1);

	v.Resize(2);
	EXPECT_EQ(ToString(v), "5,0,");
}

TEST_F(VectorTest, ResizeWithOwnElement) {
	utl::Vector<Tracked> v(&delegate);
	v.EmplaceBack(9);
	v.Fit();

	v.Resize(4, v[0]);

	EXPECT_EQ(ToString(v), "9,9,9,9,");
}

TEST_F(VectorTest, ResizeDoesNotMoveFromArgument) {
	utl::Vector<Tracked> v(&delegate);
	Tracked value(3);

	v.Resize(3, value);

	EXPECT_EQ(ToString(v), "3,3,3,");
	EXPECT_EQ(value.value, 3);
}

TEST_F(VectorTest, FitAndShrinkTo) {
	utl::Vector<int> v(&delegate);
	v.Reserve(20);
	v.PushBack(1);
	v.PushBack(2);

	v.ShrinkTo(10);
	EXPECT_EQ(v.Capacity(), 10u);

	// 要素数より小さくはならない
	v.ShrinkTo(1);
	EXPECT_EQ(v.Capacity(), 2u);
	EXPECT_EQ(ToString(v), "1,2,");

	v.Reserve(10);
	v.Fit();
	EXPECT_EQ(v.Capacity(), 2u);
	EXPECT_EQ(ToString(v), "1,2,");
}

TEST_F(VectorTest, FitEmptyFreesMemory) {
	utl::Vector<int> v(&delegate);
	v.PushBack(1);
	v.Clear();

	v.Fit();

	EXPECT_EQ(v.Capacity(), 0u);
	EXPECT_EQ(v.Begin(), nullptr);
}

TEST_F(VectorTest, ReserveKeepsElements) {
	utl::Vector<Tracked> v(&delegate);
	v.EmplaceBack(1);
	v.EmplaceBack(2);

	v.Reserve(50);

	EXPECT_EQ(v.Capacity(), 50u);
	EXPECT_EQ(ToString(v), "1,2,");
	EXPECT_EQ(Tracked::alive, 2);
}

//----------------------------------------
// オーバーヘッド
//----------------------------------------

TEST_F(VectorTest, ZeroCapacityDoesNotAllocate) {
	utl::Vector<int> v(0, &delegate);
	v.Reserve(0);

	EXPECT_EQ(v.Capacity(), 0u);
	EXPECT_EQ(alloc.allocCount, 0);
}

TEST_F(VectorTest, CopyAssignReusesMemory) {
	utl::Vector<Tracked> src(&delegate);
	src.EmplaceBack(1);
	src.EmplaceBack(2);
	utl::Vector<Tracked> dst(&delegate);
	dst.Reserve(10);
	dst.EmplaceBack(9);
	const Tracked* begin = dst.Begin();
	const int allocCount = alloc.allocCount;

	dst = src;

	EXPECT_EQ(ToString(dst), "1,2,");
	EXPECT_EQ(dst.Begin(), begin);
	EXPECT_EQ(dst.Capacity(), 10u);
	EXPECT_EQ(alloc.allocCount, allocCount);
}

TEST_F(VectorTest, CopyAssignGrowsToExactSize) {
	utl::Vector<int> src(&delegate);
	for (int i = 0; i < 5; ++i)
	{
		src.PushBack(i);
	}
	utl::Vector<int> dst(&delegate);
	dst.PushBack(9);

	dst = src;

	EXPECT_EQ(ToString(dst), "0,1,2,3,4,");
	EXPECT_EQ(dst.Capacity(), 5u);
}

TEST_F(VectorTest, InsertMiddleDoesNotMakeTemporary) {
	utl::Vector<Tracked> v(&delegate);
	v.Reserve(10);
	v.EmplaceBack(1);
	v.EmplaceBack(3);
	Tracked value(2);
	Tracked::copies = 0;
	Tracked::moves = 0;

	v.Insert(1, value);
	// ずらすムーブ1回と、直接作るコピー1回だけ
	EXPECT_EQ(Tracked::copies, 1);
	EXPECT_EQ(Tracked::moves, 1);

	Tracked moved(5);
	Tracked::copies = 0;
	Tracked::moves = 0;

	v.Insert(1, utl::Move(moved));
	// ずらすムーブ2回と、直接作るムーブ1回だけ
	EXPECT_EQ(Tracked::copies, 0);
	EXPECT_EQ(Tracked::moves, 3);

	EXPECT_EQ(ToString(v), "1,5,2,3,");
}

TEST_F(VectorTest, ResizeLargeAllocatesExactSize) {
	utl::Vector<int> v(&delegate);

	v.Resize(1000);

	EXPECT_EQ(v.Capacity(), 1000u);
	EXPECT_EQ(alloc.allocCount, 1);
}

TEST_F(VectorTest, PushBackGrowthIsAmortized) {
	utl::Vector<int> v(&delegate);
	for (int i = 0; i < 10000; ++i)
	{
		v.PushBack(i);
	}

	// 1.5倍ずつ広がるので確保の回数は少ない
	EXPECT_LT(alloc.allocCount, 30);
}
