// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Container/Set/Bit/ResizableBitSet.h"

#include <gtest/gtest.h>

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

TEST(ResizableBitSetTest, DefaultIsEmpty) {
	utl::ResizableBitSet<64> b;
	EXPECT_TRUE(b.IsEmpty());
	EXPECT_EQ(b.GetBitSetNum(), 0u);
	EXPECT_EQ(b.GetBitsNum(), 0u);
	EXPECT_FALSE(b.Test(0));
	EXPECT_FALSE(b.Test(1000));
}

TEST(ResizableBitSetTest, SetTestResetBoundary64) {
	utl::ResizableBitSet<64> b;
	b.Set(63);
	EXPECT_EQ(b.GetBitSetNum(), 1u);
	b.Set(64);
	EXPECT_EQ(b.GetBitSetNum(), 2u);
	EXPECT_TRUE(b.Test(63));
	EXPECT_TRUE(b.Test(64));
	EXPECT_FALSE(b.Test(62));
	EXPECT_FALSE(b.Test(65));
	EXPECT_FALSE(b.IsEmpty());

	b.Reset(63);
	EXPECT_FALSE(b.Test(63));
	EXPECT_TRUE(b.Test(64));
	// 立っていないビットのResetは何も起きない
	b.Reset(10);
	EXPECT_TRUE(b.Test(64));
}

TEST(ResizableBitSetTest, BoundaryOfBlockBitNum) {
	constexpr uint32_t N = 100;
	utl::ResizableBitSet<N> b;
	b.Set(N - 1);
	EXPECT_EQ(b.GetBitSetNum(), 1u);
	EXPECT_TRUE(b.Test(N - 1));
	EXPECT_FALSE(b.Test(N));
	b.Set(N);
	EXPECT_EQ(b.GetBitSetNum(), 2u);
	EXPECT_TRUE(b.Test(N));
	EXPECT_FALSE(b.Test(N - 2));
	EXPECT_FALSE(b.Test(N + 1));
}

TEST(ResizableBitSetTest, FarIndex) {
	utl::ResizableBitSet<64> b;
	b.Set(10000);
	EXPECT_EQ(b.GetBitSetNum(), 10000u / 64 + 1);
	EXPECT_TRUE(b.Test(10000));
	EXPECT_FALSE(b.Test(9999));
	EXPECT_FALSE(b.Test(0));
	b.Set(0);
	EXPECT_TRUE(b.Test(0));
	EXPECT_TRUE(b.Test(10000));
}

TEST(ResizableBitSetTest, OutOfRangeResetAndTest) {
	utl::ResizableBitSet<64> b;
	b.Set(5);
	b.Reset(100000);
	b.Reset(0xFFFFFFFFu);
	EXPECT_FALSE(b.Test(100000));
	EXPECT_FALSE(b.Test(0xFFFFFFFFu));
	EXPECT_EQ(b.GetBitSetNum(), 1u);
	EXPECT_TRUE(b.Test(5));
}

TEST(ResizableBitSetTest, ClearAndGetters) {
	utl::ResizableBitSet<64> b;
	b.Set(200);
	EXPECT_EQ(b.GetBitSetNum(), 4u);
	EXPECT_EQ(b.GetBitsNum(), 4u * 64);
	EXPECT_EQ(b.GetData().Size(), 4u);
	EXPECT_TRUE(b.GetData()[3].Test(200 - 192));
	b.Clear();
	EXPECT_EQ(b.GetBitSetNum(), 0u);
	EXPECT_EQ(b.GetBitsNum(), 0u);
	EXPECT_FALSE(b.Test(200));
	EXPECT_TRUE(b.IsEmpty());
}

TEST(ResizableBitSetTest, IsEmptyWithAllZeroBlocks) {
	utl::ResizableBitSet<64> b;
	b.Set(200);
	b.Reset(200);
	EXPECT_EQ(b.GetBitSetNum(), 4u);
	EXPECT_TRUE(b.IsEmpty());
}

TEST(ResizableBitSetTest, CopyIsIndependent) {
	utl::ResizableBitSet<64> a;
	a.Set(1);
	a.Set(100);
	utl::ResizableBitSet<64> b(a);
	EXPECT_TRUE(b.Test(1));
	EXPECT_TRUE(b.Test(100));
	b.Set(2);
	a.Reset(1);
	EXPECT_TRUE(a.Test(100));
	EXPECT_FALSE(a.Test(2));
	EXPECT_TRUE(b.Test(1));
	EXPECT_TRUE(b.Test(2));

	utl::ResizableBitSet<64> c;
	c = b;
	c.Reset(100);
	EXPECT_TRUE(b.Test(100));
	EXPECT_FALSE(c.Test(100));
}

TEST(ResizableBitSetTest, SelfAssign) {
	utl::ResizableBitSet<64> a;
	a.Set(70);
	utl::ResizableBitSet<64>& ref = a;
	a = ref;
	EXPECT_TRUE(a.Test(70));
	EXPECT_EQ(a.GetBitSetNum(), 2u);
	a = std::move(ref);
	EXPECT_TRUE(a.Test(70));
}

TEST(ResizableBitSetTest, MoveEmptiesSource) {
	utl::ResizableBitSet<64> a;
	a.Set(3);
	a.Set(130);
	utl::ResizableBitSet<64> b(std::move(a));
	EXPECT_TRUE(b.Test(3));
	EXPECT_TRUE(b.Test(130));
	EXPECT_EQ(a.GetBitSetNum(), 0u);
	EXPECT_FALSE(a.Test(3));

	utl::ResizableBitSet<64> c;
	c.Set(1);
	c = std::move(b);
	EXPECT_TRUE(c.Test(3));
	EXPECT_TRUE(c.Test(130));
	EXPECT_FALSE(c.Test(1));
	EXPECT_EQ(b.GetBitSetNum(), 0u);
}

TEST(ResizableBitSetTest, EqualityIgnoresTrailingZeroBlocks) {
	utl::ResizableBitSet<64> a;
	utl::ResizableBitSet<64> b;
	a.Set(5);
	b.Set(5);
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a != b);

	b.Set(500);
	EXPECT_FALSE(a == b);
	EXPECT_TRUE(a != b);
	EXPECT_FALSE(b == a);

	// 末尾が全て0ならサイズが違っても等しい
	b.Reset(500);
	EXPECT_NE(a.GetBitSetNum(), b.GetBitSetNum());
	EXPECT_TRUE(a == b);
	EXPECT_TRUE(b == a);
	EXPECT_FALSE(a != b);

	b.Set(6);
	EXPECT_FALSE(a == b);
}

TEST(ResizableBitSetTest, EmptySetsAreEqual) {
	utl::ResizableBitSet<64> a;
	utl::ResizableBitSet<64> b;
	b.Set(300);
	b.Reset(300);
	EXPECT_TRUE(a == b);
	EXPECT_TRUE(b == a);
}

TEST(ResizableBitSetTest, IsContain) {
	utl::ResizableBitSet<64> a;
	utl::ResizableBitSet<64> sub;
	a.Set(1);
	a.Set(70);
	a.Set(200);
	sub.Set(1);
	sub.Set(200);
	EXPECT_TRUE(a.IsContain(sub));
	EXPECT_FALSE(sub.IsContain(a));
	EXPECT_TRUE(a.IsContain(a));

	sub.Set(2);
	EXPECT_FALSE(a.IsContain(sub));
}

TEST(ResizableBitSetTest, IsContainLongerSource) {
	utl::ResizableBitSet<64> a;
	utl::ResizableBitSet<64> src;
	a.Set(1);
	src.Set(1);
	src.Set(500);
	EXPECT_FALSE(a.IsContain(src));

	// srcが長くても、はみ出した部分が全て0なら含む
	src.Reset(500);
	EXPECT_TRUE(a.IsContain(src));
}

TEST(ResizableBitSetTest, IsContainEmptySet) {
	utl::ResizableBitSet<64> a;
	utl::ResizableBitSet<64> empty;
	EXPECT_TRUE(a.IsContain(empty));
	EXPECT_TRUE(empty.IsContain(empty));
	a.Set(10);
	EXPECT_TRUE(a.IsContain(empty));
	EXPECT_FALSE(empty.IsContain(a));
}

TEST(ResizableBitSetTest, BlockTypeUint8) {
	utl::ResizableBitSet<16, uint8_t> b;
	b.Set(15);
	b.Set(16);
	b.Set(40);
	EXPECT_EQ(b.GetBitSetNum(), 3u);
	EXPECT_TRUE(b.Test(15));
	EXPECT_TRUE(b.Test(16));
	EXPECT_TRUE(b.Test(40));
	EXPECT_FALSE(b.Test(17));
	b.Reset(16);
	EXPECT_FALSE(b.Test(16));
}

TEST(ResizableBitSetTest, BlockTypeUint32) {
	utl::ResizableBitSet<64, uint32_t> b;
	b.Set(31);
	b.Set(32);
	b.Set(63);
	b.Set(64);
	EXPECT_EQ(b.GetBitSetNum(), 2u);
	EXPECT_TRUE(b.Test(31));
	EXPECT_TRUE(b.Test(32));
	EXPECT_TRUE(b.Test(63));
	EXPECT_TRUE(b.Test(64));
	EXPECT_FALSE(b.Test(33));
}

TEST(ResizableBitSetTest, BlockBitNumNotMultipleOfWord) {
	utl::ResizableBitSet<10, uint8_t> a;
	a.Set(9);
	a.Set(10);
	a.Set(19);
	EXPECT_EQ(a.GetBitSetNum(), 2u);
	EXPECT_EQ(a.GetBitsNum(), 20u);
	EXPECT_TRUE(a.Test(9));
	EXPECT_TRUE(a.Test(10));
	EXPECT_TRUE(a.Test(19));
	EXPECT_FALSE(a.Test(8));
	EXPECT_FALSE(a.Test(11));

	utl::ResizableBitSet<70> b;
	b.Set(69);
	b.Set(70);
	EXPECT_EQ(b.GetBitSetNum(), 2u);
	EXPECT_TRUE(b.Test(69));
	EXPECT_TRUE(b.Test(70));
	EXPECT_FALSE(b.Test(68));
	EXPECT_FALSE(b.Test(71));
	b.Reset(69);
	EXPECT_FALSE(b.Test(69));
	EXPECT_TRUE(b.Test(70));
}

TEST(ResizableBitSetTest, UsesGivenAllocator) {
	CountingAllocator alloc;
	utl::MemoryAllocatorDelegate delegate;
	delegate.Register<CountingAllocator, &CountingAllocator::Allocate, &CountingAllocator::Free>(&alloc);
	{
		utl::ResizableBitSet<64> b{ utl::MemoryAllocatorRef(&delegate) };
		b.Set(10);
		b.Set(1000);
		EXPECT_GT(alloc.allocCount, 0);
		EXPECT_TRUE(b.Test(1000));
	}
	EXPECT_EQ(alloc.allocCount, alloc.freeCount);
}
