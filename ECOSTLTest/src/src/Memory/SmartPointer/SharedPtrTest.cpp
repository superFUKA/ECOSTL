#include <gtest/gtest.h>

#include <utility>

#include "ECOSTL/src/Memory/SmartPointer/SharedPtr.h"

namespace
{
	/// <summary>
	/// 生成と破棄の回数を数えるテスト用の型
	/// </summary>
	struct Tracked
	{
		static inline int alive = 0;
		static inline int destroyed = 0;

		static void ResetCount()
		{
			alive = 0;
			destroyed = 0;
		}

		Tracked(int v = 0, int w = 0)
			: value(v)
			, value2(w)
		{
			++alive;
		}
		Tracked(const Tracked& src)
			: value(src.value)
			, value2(src.value2)
		{
			++alive;
		}
		virtual ~Tracked()
		{
			--alive;
			++destroyed;
		}

		int value;
		int value2;
	};

	struct DerivedTracked : Tracked
	{
		static inline int derivedDestroyed = 0;

		DerivedTracked(int v)
			: Tracked(v)
		{}
		~DerivedTracked() override
		{
			++derivedDestroyed;
		}
	};

	/// <summary>
	/// 確保と開放の回数を数えるテスト用のアロケータ
	/// </summary>
	class CountingAllocator
	{
	public:
		void* Allocate(uint32_t size, uint32_t)
		{
			++allocCount;
			lastSize = size;
			lastAllocated = malloc(size);
			return lastAllocated;
		}
		bool Free(void* memory)
		{
			++freeCount;
			lastFreed = memory;
			free(memory);
			return true;
		}

		int allocCount = 0;
		int freeCount = 0;
		uint32_t lastSize = 0;
		void* lastAllocated = nullptr;
		void* lastFreed = nullptr;
	};

	class SharedPtrTest : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			Tracked::ResetCount();
			DerivedTracked::derivedDestroyed = 0;
			delegate.Register<CountingAllocator, &CountingAllocator::Allocate, &CountingAllocator::Free>(&alloc);
		}

		CountingAllocator alloc;
		utl::MemoryAllocatorDelegate delegate;
	};
}

//----------------------------------------
// 作成
//----------------------------------------

TEST_F(SharedPtrTest, DefaultIsEmpty) {
	utl::SharedPtr<Tracked> ptr;

	EXPECT_FALSE(ptr);
	EXPECT_EQ(ptr.Get(), nullptr);
}

TEST_F(SharedPtrTest, NullptrConstructIsEmpty) {
	utl::SharedPtr<Tracked> ptr(nullptr);

	EXPECT_FALSE(ptr);
	EXPECT_EQ(ptr.Get(), nullptr);
}

TEST_F(SharedPtrTest, MakeSharedPassesArguments) {
	utl::SharedPtr<Tracked> ptr = utl::MakeShared<Tracked>(3, 4);

	ASSERT_TRUE(ptr);
	EXPECT_EQ(ptr->value, 3);
	EXPECT_EQ(ptr->value2, 4);
	EXPECT_EQ((*ptr).value, 3);
	EXPECT_EQ(Tracked::alive, 1);
}

TEST_F(SharedPtrTest, MakeSharedWithoutArguments) {
	utl::SharedPtr<Tracked> ptr = utl::MakeShared<Tracked>();

	ASSERT_TRUE(ptr);
	EXPECT_EQ(ptr->value, 0);
}

TEST_F(SharedPtrTest, MakeSharedWithAllocAllocatesOnce) {
	{
		utl::SharedPtr<Tracked> ptr = utl::MakeSharedWithAlloc<Tracked>(&delegate, 7);

		ASSERT_TRUE(ptr);
		EXPECT_EQ(ptr->value, 7);
		// インスタンスとカウンタをまとめて1回で確保している
		EXPECT_EQ(alloc.allocCount, 1);
		EXPECT_EQ(alloc.lastSize, sizeof(utl::RefCounterToIns<Tracked>));
		EXPECT_EQ(alloc.freeCount, 0);
	}

	EXPECT_EQ(alloc.freeCount, 1);
	EXPECT_EQ(alloc.lastFreed, alloc.lastAllocated);
	EXPECT_EQ(Tracked::alive, 0);
}

TEST_F(SharedPtrTest, WritesThroughPointer) {
	utl::SharedPtr<Tracked> ptr = utl::MakeShared<Tracked>(1);

	ptr->value = 10;
	(*ptr).value2 = 20;

	EXPECT_EQ(ptr.Get()->value, 10);
	EXPECT_EQ(ptr.Get()->value2, 20);
}

//----------------------------------------
// コピー
//----------------------------------------

TEST_F(SharedPtrTest, CopyConstructSharesInstance) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> b(a);

	EXPECT_TRUE(a);
	EXPECT_TRUE(b);
	EXPECT_EQ(a.Get(), b.Get());
	EXPECT_EQ(Tracked::alive, 1);
}

TEST_F(SharedPtrTest, CopyConstructFromEmpty) {
	utl::SharedPtr<Tracked> a;
	utl::SharedPtr<Tracked> b(a);

	EXPECT_FALSE(b);
}

TEST_F(SharedPtrTest, ChangesAreVisibleFromCopies) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> b = a;

	b->value = 42;

	EXPECT_EQ(a->value, 42);
}

TEST_F(SharedPtrTest, ReleasedWhenLastReferenceGone) {
	utl::SharedPtr<Tracked> a = utl::MakeSharedWithAlloc<Tracked>(&delegate, 1);
	{
		utl::SharedPtr<Tracked> b = a;
		utl::SharedPtr<Tracked> c = b;
		EXPECT_EQ(Tracked::alive, 1);
	}

	// まだaが参照している
	EXPECT_EQ(Tracked::alive, 1);
	EXPECT_EQ(alloc.freeCount, 0);

	a.Reset();
	EXPECT_EQ(Tracked::alive, 0);
	EXPECT_EQ(Tracked::destroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(SharedPtrTest, CopyAssignSharesInstance) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> b;

	b = a;

	EXPECT_EQ(a.Get(), b.Get());
	EXPECT_EQ(Tracked::alive, 1);

	a.Reset();
	EXPECT_EQ(Tracked::alive, 1);
	EXPECT_EQ(b->value, 1);
}

TEST_F(SharedPtrTest, CopyAssignReleasesPreviousInstance) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> b = utl::MakeSharedWithAlloc<Tracked>(&delegate, 2);

	b = a;

	EXPECT_EQ(b->value, 1);
	EXPECT_EQ(Tracked::alive, 1);
	EXPECT_EQ(Tracked::destroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(SharedPtrTest, CopyAssignEmpty) {
	utl::SharedPtr<Tracked> a;
	utl::SharedPtr<Tracked> b = utl::MakeShared<Tracked>(1);

	b = a;

	EXPECT_FALSE(b);
	EXPECT_EQ(Tracked::alive, 0);
}

TEST_F(SharedPtrTest, SelfAssignKeepsInstance) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked>& ref = a;

	a = ref;

	ASSERT_TRUE(a);
	EXPECT_EQ(a->value, 1);
	EXPECT_EQ(Tracked::alive, 1);
}

TEST_F(SharedPtrTest, AssignSameInstanceKeepsCount) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> b = a;

	b = a;
	a.Reset();

	// bの参照が残っているので開放されない
	EXPECT_EQ(Tracked::alive, 1);
	b.Reset();
	EXPECT_EQ(Tracked::alive, 0);
}

//----------------------------------------
// ムーブ
//----------------------------------------

TEST_F(SharedPtrTest, MoveConstructTransfersReference) {
	utl::SharedPtr<Tracked> src = utl::MakeShared<Tracked>(5);
	Tracked* raw = src.Get();

	utl::SharedPtr<Tracked> dst(std::move(src));

	EXPECT_FALSE(src);
	EXPECT_EQ(dst.Get(), raw);

	dst.Reset();
	EXPECT_EQ(Tracked::alive, 0);
}

TEST_F(SharedPtrTest, MoveAssignReleasesPreviousInstance) {
	utl::SharedPtr<Tracked> src = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> dst = utl::MakeSharedWithAlloc<Tracked>(&delegate, 2);

	dst = std::move(src);

	EXPECT_FALSE(src);
	EXPECT_EQ(dst->value, 1);
	EXPECT_EQ(Tracked::alive, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(SharedPtrTest, MoveConstructFromDerived) {
	utl::SharedPtr<DerivedTracked> src = utl::MakeSharedWithAlloc<DerivedTracked>(&delegate, 9);
	Tracked* raw = src.Get();

	{
		utl::SharedPtr<Tracked> dst(std::move(src));

		EXPECT_FALSE(src);
		EXPECT_EQ(dst.Get(), raw);
		EXPECT_EQ(dst->value, 9);
	}

	// 基底クラスのポインタから派生クラスのデストラクタが呼ばれる
	EXPECT_EQ(DerivedTracked::derivedDestroyed, 1);
	EXPECT_EQ(Tracked::alive, 0);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(SharedPtrTest, MoveAssignFromDerived) {
	utl::SharedPtr<DerivedTracked> src = utl::MakeSharedWithAlloc<DerivedTracked>(&delegate, 9);
	utl::SharedPtr<Tracked> dst = utl::MakeShared<Tracked>(1);

	dst = std::move(src);

	EXPECT_FALSE(src);
	EXPECT_EQ(dst->value, 9);
	EXPECT_EQ(Tracked::alive, 1);

	dst.Reset();
	EXPECT_EQ(DerivedTracked::derivedDestroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

//----------------------------------------
// 開放
//----------------------------------------

TEST_F(SharedPtrTest, ResetEmptyIsSafe) {
	utl::SharedPtr<Tracked> ptr;

	ptr.Reset();

	EXPECT_FALSE(ptr);
}

TEST_F(SharedPtrTest, ResetTwiceIsSafe) {
	utl::SharedPtr<Tracked> ptr = utl::MakeSharedWithAlloc<Tracked>(&delegate, 1);

	ptr.Reset();
	ptr.Reset();

	EXPECT_EQ(Tracked::destroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(SharedPtrTest, AssignNullptrReleasesInstance) {
	utl::SharedPtr<Tracked> ptr = utl::MakeSharedWithAlloc<Tracked>(&delegate, 1);

	ptr = nullptr;

	EXPECT_FALSE(ptr);
	EXPECT_EQ(Tracked::destroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

//----------------------------------------
// 比較
//----------------------------------------

TEST_F(SharedPtrTest, CompareWithRawPointer) {
	utl::SharedPtr<Tracked> ptr = utl::MakeShared<Tracked>(1);
	Tracked other;

	EXPECT_TRUE(ptr == ptr.Get());
	EXPECT_FALSE(ptr != ptr.Get());
	EXPECT_TRUE(ptr != &other);
	EXPECT_FALSE(ptr == &other);
}

TEST_F(SharedPtrTest, EmptyNeverEqualsRawPointer) {
	utl::SharedPtr<Tracked> empty;
	Tracked other;

	EXPECT_FALSE(empty == &other);
	EXPECT_TRUE(empty != &other);
}

TEST_F(SharedPtrTest, CompareWithSharedPtr) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> copy = a;
	utl::SharedPtr<Tracked> b = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> emptyA;
	utl::SharedPtr<Tracked> emptyB;

	EXPECT_TRUE(a == copy);
	EXPECT_FALSE(a != copy);
	EXPECT_TRUE(a != b);
	EXPECT_TRUE(emptyA == emptyB);
	EXPECT_TRUE(a != emptyA);
	EXPECT_TRUE(emptyA != a);
}
