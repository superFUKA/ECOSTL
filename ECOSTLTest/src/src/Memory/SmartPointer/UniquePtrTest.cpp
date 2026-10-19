#include <gtest/gtest.h>

#include <utility>

#include "ECOSTL/src/Memory/SmartPointer/UniquePtr.h"

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
			return malloc(size);
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
		void* lastFreed = nullptr;
	};

	class UniquePtrTest : public ::testing::Test
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

TEST_F(UniquePtrTest, DefaultIsEmpty) {
	utl::UniquePtr<Tracked> ptr;

	EXPECT_FALSE(ptr);
	EXPECT_EQ(ptr.Get(), nullptr);
	EXPECT_EQ(ptr.MemAllocator().Get(), nullptr);
}

TEST_F(UniquePtrTest, NullptrConstructIsEmpty) {
	utl::UniquePtr<Tracked> ptr(nullptr);

	EXPECT_FALSE(ptr);
	EXPECT_EQ(ptr.Get(), nullptr);
}

TEST_F(UniquePtrTest, MakeUniquePassesArguments) {
	utl::UniquePtr<Tracked> ptr = utl::MakeUnique<Tracked>(3, 4);

	ASSERT_TRUE(ptr);
	EXPECT_EQ(ptr->value, 3);
	EXPECT_EQ(ptr->value2, 4);
	EXPECT_EQ((*ptr).value, 3);
	EXPECT_EQ(Tracked::alive, 1);
}

TEST_F(UniquePtrTest, MakeUniqueWithoutArguments) {
	utl::UniquePtr<Tracked> ptr = utl::MakeUnique<Tracked>();

	ASSERT_TRUE(ptr);
	EXPECT_EQ(ptr->value, 0);
}

TEST_F(UniquePtrTest, MakeUniqueWithAllocUsesAllocator) {
	{
		utl::UniquePtr<Tracked> ptr = utl::MakeUniqueWithAlloc<Tracked>(&delegate, 7);

		ASSERT_TRUE(ptr);
		EXPECT_EQ(ptr->value, 7);
		EXPECT_EQ(alloc.allocCount, 1);
		EXPECT_EQ(alloc.lastSize, sizeof(Tracked));
		EXPECT_EQ(ptr.MemAllocator().Get(), &delegate);
		EXPECT_EQ(alloc.freeCount, 0);
	}

	EXPECT_EQ(alloc.freeCount, 1);
	EXPECT_EQ(Tracked::alive, 0);
}

TEST_F(UniquePtrTest, WritesThroughPointer) {
	utl::UniquePtr<Tracked> ptr = utl::MakeUnique<Tracked>(1);

	ptr->value = 10;
	(*ptr).value2 = 20;

	EXPECT_EQ(ptr.Get()->value, 10);
	EXPECT_EQ(ptr.Get()->value2, 20);
}

//----------------------------------------
// 開放
//----------------------------------------

TEST_F(UniquePtrTest, DestructorReleasesInstance) {
	{
		utl::UniquePtr<Tracked> ptr = utl::MakeUnique<Tracked>(1);
		EXPECT_EQ(Tracked::alive, 1);
	}

	EXPECT_EQ(Tracked::alive, 0);
	EXPECT_EQ(Tracked::destroyed, 1);
}

TEST_F(UniquePtrTest, ResetReleasesInstance) {
	utl::UniquePtr<Tracked> ptr = utl::MakeUniqueWithAlloc<Tracked>(&delegate, 1);
	Tracked* raw = ptr.Get();

	ptr.Reset();

	EXPECT_FALSE(ptr);
	EXPECT_EQ(ptr.Get(), nullptr);
	EXPECT_EQ(ptr.MemAllocator().Get(), nullptr);
	EXPECT_EQ(Tracked::destroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
	EXPECT_EQ(alloc.lastFreed, static_cast<void*>(raw));
}

TEST_F(UniquePtrTest, ResetTwiceIsSafe) {
	utl::UniquePtr<Tracked> ptr = utl::MakeUniqueWithAlloc<Tracked>(&delegate, 1);

	ptr.Reset();
	ptr.Reset();

	EXPECT_EQ(Tracked::destroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(UniquePtrTest, ResetEmptyIsSafe) {
	utl::UniquePtr<Tracked> ptr;

	ptr.Reset();

	EXPECT_FALSE(ptr);
	EXPECT_EQ(Tracked::destroyed, 0);
}

TEST_F(UniquePtrTest, AssignNullptrReleasesInstance) {
	utl::UniquePtr<Tracked> ptr = utl::MakeUniqueWithAlloc<Tracked>(&delegate, 1);

	ptr = nullptr;

	EXPECT_FALSE(ptr);
	EXPECT_EQ(Tracked::destroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

//----------------------------------------
// ムーブ
//----------------------------------------

TEST_F(UniquePtrTest, MoveConstructTransfersOwnership) {
	utl::UniquePtr<Tracked> src = utl::MakeUniqueWithAlloc<Tracked>(&delegate, 5);
	Tracked* raw = src.Get();

	utl::UniquePtr<Tracked> dst(std::move(src));

	EXPECT_FALSE(src);
	EXPECT_EQ(src.MemAllocator().Get(), nullptr);
	EXPECT_EQ(dst.Get(), raw);
	EXPECT_EQ(dst.MemAllocator().Get(), &delegate);
	EXPECT_EQ(Tracked::alive, 1);
	EXPECT_EQ(alloc.freeCount, 0);
}

TEST_F(UniquePtrTest, MoveAssignTransfersOwnership) {
	utl::UniquePtr<Tracked> src = utl::MakeUniqueWithAlloc<Tracked>(&delegate, 5);
	Tracked* raw = src.Get();
	utl::UniquePtr<Tracked> dst;

	dst = std::move(src);

	EXPECT_FALSE(src);
	EXPECT_EQ(dst.Get(), raw);
	EXPECT_EQ(dst.MemAllocator().Get(), &delegate);
	EXPECT_EQ(Tracked::alive, 1);
}

TEST_F(UniquePtrTest, MoveAssignReleasesPreviousInstance) {
	utl::UniquePtr<Tracked> src = utl::MakeUnique<Tracked>(1);
	utl::UniquePtr<Tracked> dst = utl::MakeUniqueWithAlloc<Tracked>(&delegate, 2);

	dst = std::move(src);

	EXPECT_EQ(dst->value, 1);
	EXPECT_EQ(Tracked::alive, 1);
	EXPECT_EQ(Tracked::destroyed, 1);
	// 元のインスタンスは確保したアロケータで開放されている
	EXPECT_EQ(alloc.freeCount, 1);
	// 標準の手段で確保したものを引き継いでいる
	EXPECT_EQ(dst.MemAllocator().Get(), nullptr);
}

TEST_F(UniquePtrTest, MoveFromEmpty) {
	utl::UniquePtr<Tracked> src;
	utl::UniquePtr<Tracked> dst = utl::MakeUnique<Tracked>(1);

	dst = std::move(src);

	EXPECT_FALSE(dst);
	EXPECT_EQ(Tracked::alive, 0);
}

TEST_F(UniquePtrTest, MoveConstructFromDerived) {
	utl::UniquePtr<DerivedTracked> src = utl::MakeUniqueWithAlloc<DerivedTracked>(&delegate, 9);
	Tracked* raw = src.Get();

	{
		utl::UniquePtr<Tracked> dst(std::move(src));

		EXPECT_FALSE(src);
		EXPECT_EQ(src.MemAllocator().Get(), nullptr);
		EXPECT_EQ(dst.Get(), raw);
		EXPECT_EQ(dst->value, 9);
		EXPECT_EQ(dst.MemAllocator().Get(), &delegate);
	}

	// 基底クラスのポインタから派生クラスのデストラクタが呼ばれる
	EXPECT_EQ(DerivedTracked::derivedDestroyed, 1);
	EXPECT_EQ(Tracked::alive, 0);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(UniquePtrTest, MoveAssignFromDerived) {
	utl::UniquePtr<DerivedTracked> src = utl::MakeUniqueWithAlloc<DerivedTracked>(&delegate, 9);
	utl::UniquePtr<Tracked> dst = utl::MakeUnique<Tracked>(1);

	dst = std::move(src);

	EXPECT_FALSE(src);
	EXPECT_EQ(dst->value, 9);
	EXPECT_EQ(dst.MemAllocator().Get(), &delegate);
	EXPECT_EQ(Tracked::alive, 1);

	dst.Reset();
	EXPECT_EQ(DerivedTracked::derivedDestroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

//----------------------------------------
// 比較
//----------------------------------------

TEST_F(UniquePtrTest, CompareWithRawPointer) {
	utl::UniquePtr<Tracked> ptr = utl::MakeUnique<Tracked>(1);
	Tracked other;

	EXPECT_TRUE(ptr == ptr.Get());
	EXPECT_FALSE(ptr != ptr.Get());
	EXPECT_TRUE(ptr != &other);
	EXPECT_FALSE(ptr == &other);
}

TEST_F(UniquePtrTest, CompareWithNullptr) {
	utl::UniquePtr<Tracked> empty;
	utl::UniquePtr<Tracked> ptr = utl::MakeUnique<Tracked>(1);

	EXPECT_TRUE(empty == nullptr);
	EXPECT_FALSE(empty != nullptr);
	EXPECT_TRUE(ptr != nullptr);
	EXPECT_FALSE(ptr == nullptr);
}

TEST_F(UniquePtrTest, CompareWithUniquePtr) {
	utl::UniquePtr<Tracked> a = utl::MakeUnique<Tracked>(1);
	utl::UniquePtr<Tracked> b = utl::MakeUnique<Tracked>(1);
	utl::UniquePtr<Tracked> emptyA;
	utl::UniquePtr<Tracked> emptyB;

	EXPECT_TRUE(a == a);
	EXPECT_TRUE(a != b);
	EXPECT_FALSE(a == b);
	EXPECT_TRUE(emptyA == emptyB);
	EXPECT_TRUE(a != emptyA);
}
