// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Container/Set/Bit/SparseBitSet.h"

#include <gtest/gtest.h>

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

	utl::SparseBitSet Make(std::initializer_list<uint32_t> indeces)
	{
		utl::SparseBitSet s;
		for (uint32_t i : indeces)
		{
			s.Set(i);
		}
		return s;
	}

	bool IsAscending(const utl::SparseBitSet& s)
	{
		const auto& v = s.GetBlockIndeces();
		for (uint32_t i = 1; i < v.Size(); ++i)
		{
			if (v[i - 1] >= v[i])
			{
				return false;
			}
		}
		return true;
	}
}

TEST(SparseBitSetTest, InitialEmpty)
{
	utl::SparseBitSet s;
	EXPECT_TRUE(s.IsEmpty());
	EXPECT_EQ(s.BlockCount(), 0u);
	EXPECT_FALSE(s.Test(0));
	EXPECT_FALSE(s[12345]);
}

TEST(SparseBitSetTest, SetTestReset)
{
	utl::SparseBitSet s;
	s.Set(5);
	EXPECT_FALSE(s.IsEmpty());
	EXPECT_TRUE(s.Test(5));
	EXPECT_TRUE(s[5]);
	EXPECT_FALSE(s.Test(4));
	s.Set(5);	// 二重に立てても変わらない
	EXPECT_EQ(s.BlockCount(), 1u);
	s.Reset(4);	// 立っていないビットのリセットは何も起きない
	s.Reset(1000);
	EXPECT_TRUE(s.Test(5));
	s.Reset(5);
	EXPECT_FALSE(s.Test(5));
	EXPECT_TRUE(s.IsEmpty());
}

TEST(SparseBitSetTest, Clear)
{
	utl::SparseBitSet s = Make({ 1, 100, 10000 });
	s.Clear();
	EXPECT_TRUE(s.IsEmpty());
	EXPECT_EQ(s.BlockCount(), 0u);
	EXPECT_FALSE(s.Test(100));
}

TEST(SparseBitSetTest, BlockBoundary)
{
	utl::SparseBitSet s = Make({ 31, 32, 33 });
	EXPECT_EQ(s.BlockCount(), 2u);
	EXPECT_TRUE(s.Test(31));
	EXPECT_TRUE(s.Test(32));
	EXPECT_TRUE(s.Test(33));
	EXPECT_FALSE(s.Test(30));
	EXPECT_FALSE(s.Test(34));
	s.Reset(31);
	EXPECT_EQ(s.BlockCount(), 1u);
	s.Reset(32);
	EXPECT_EQ(s.BlockCount(), 1u);
	s.Reset(33);
	EXPECT_TRUE(s.IsEmpty());
}

TEST(SparseBitSetTest, FarIndices)
{
	utl::SparseBitSet s = Make({ 0, 1000000, 50 });
	EXPECT_EQ(s.BlockCount(), 3u);
	EXPECT_TRUE(s.Test(0));
	EXPECT_TRUE(s.Test(50));
	EXPECT_TRUE(s.Test(1000000));
	EXPECT_FALSE(s.Test(999999));
}

TEST(SparseBitSetTest, BlockIndecesAscendingInAnyOrder)
{
	utl::SparseBitSet desc = Make({ 500, 400, 300, 200, 100, 0 });
	EXPECT_TRUE(IsAscending(desc));
	EXPECT_EQ(desc.BlockCount(), 6u);

	utl::SparseBitSet mixed = Make({ 300, 0, 500, 100, 400, 200 });
	EXPECT_TRUE(IsAscending(mixed));
	EXPECT_TRUE(desc == mixed);

	const auto& indeces = mixed.GetBlockIndeces();
	EXPECT_EQ(indeces.Size(), mixed.GetBitBlocks().Size());
	EXPECT_EQ(indeces[0], 0u);
	EXPECT_EQ(indeces[5], 500u >> 5);
}

TEST(SparseBitSetTest, RemovingLastBitErasesBlock)
{
	utl::SparseBitSet s = Make({ 1, 2, 100, 200 });
	EXPECT_EQ(s.BlockCount(), 3u);
	s.Reset(100);
	EXPECT_EQ(s.BlockCount(), 2u);
	EXPECT_FALSE(s.Test(100));
	EXPECT_TRUE(s.Test(1));
	EXPECT_TRUE(s.Test(200));
	EXPECT_TRUE(IsAscending(s));
	EXPECT_EQ(s.GetBitBlocks().Size(), s.GetBlockIndeces().Size());
}

TEST(SparseBitSetTest, LargeIndexAbove2Pow21)
{
	// ブロック番号が 16bit に収まらない領域で別のビットが立たないこと
	const uint32_t big = (1u << 21) + 5;
	utl::SparseBitSet s;
	s.Set(big);
	EXPECT_TRUE(s.Test(big));
	EXPECT_FALSE(s.Test(5));
	s.Set(5);
	EXPECT_EQ(s.BlockCount(), 2u);
	s.Reset(big);
	EXPECT_FALSE(s.Test(big));
	EXPECT_TRUE(s.Test(5));
	EXPECT_EQ(s.BlockCount(), 1u);

	utl::SparseBitSet t = Make({ 1u << 24, (1u << 24) + 1, 0xFFFFFFFFu });
	EXPECT_TRUE(t.Test(0xFFFFFFFFu));
	EXPECT_FALSE(t.Test(0));
	EXPECT_EQ(t.BlockCount(), 2u);
}

TEST(SparseBitSetTest, Or)
{
	utl::SparseBitSet a = Make({ 1, 100, 1000 });
	utl::SparseBitSet b = Make({ 1, 50, 2000 });
	utl::SparseBitSet r = a | b;
	EXPECT_TRUE(r == Make({ 1, 50, 100, 1000, 2000 }));
	EXPECT_TRUE(IsAscending(r));
	EXPECT_TRUE(a == Make({ 1, 100, 1000 }));	// 非破壊
	a |= b;
	EXPECT_TRUE(a == r);
	a |= a;
	EXPECT_TRUE(a == r);
}

TEST(SparseBitSetTest, And)
{
	utl::SparseBitSet a = Make({ 1, 2, 100, 1000 });
	utl::SparseBitSet b = Make({ 2, 3, 1000, 5000 });
	EXPECT_TRUE((a & b) == Make({ 2, 1000 }));
	utl::SparseBitSet c = Make({ 1, 100 });
	utl::SparseBitSet d = Make({ 2, 101 });
	EXPECT_TRUE((c & d).IsEmpty());	// 同じブロックで重ならない
	EXPECT_TRUE((a & utl::SparseBitSet()).IsEmpty());
	EXPECT_TRUE((utl::SparseBitSet() & a).IsEmpty());
	a &= b;
	EXPECT_TRUE(a == Make({ 2, 1000 }));
	a &= a;
	EXPECT_TRUE(a == Make({ 2, 1000 }));
}

TEST(SparseBitSetTest, Xor)
{
	utl::SparseBitSet a = Make({ 1, 2, 100, 1000 });
	utl::SparseBitSet b = Make({ 2, 100, 5000 });
	utl::SparseBitSet r = a ^ b;
	EXPECT_TRUE(r == Make({ 1, 1000, 5000 }));
	EXPECT_TRUE(IsAscending(r));
	a ^= b;
	EXPECT_TRUE(a == r);
}

TEST(SparseBitSetTest, AndNot)
{
	utl::SparseBitSet a = Make({ 1, 2, 100, 1000 });
	utl::SparseBitSet b = Make({ 2, 100, 5000 });
	EXPECT_TRUE((a - b) == Make({ 1, 1000 }));
	EXPECT_TRUE((b - a) == Make({ 5000 }));
	a -= b;
	EXPECT_TRUE(a == Make({ 1, 1000 }));
}

TEST(SparseBitSetTest, SelfOperations)
{
	utl::SparseBitSet a = Make({ 1, 100, 1000 });
	utl::SparseBitSet x = a;
	x ^= x;
	EXPECT_TRUE(x.IsEmpty());
	EXPECT_EQ(x.BlockCount(), 0u);

	utl::SparseBitSet y = a;
	y -= y;
	EXPECT_TRUE(y.IsEmpty());
	EXPECT_EQ(y.GetBitBlocks().Size(), 0u);

	EXPECT_TRUE((a ^ a).IsEmpty());
	EXPECT_TRUE((a - a).IsEmpty());
	EXPECT_TRUE((a & a) == a);
	EXPECT_TRUE((a | a) == a);
}

TEST(SparseBitSetTest, Equality)
{
	utl::SparseBitSet a = Make({ 1, 100 });
	EXPECT_TRUE(a == Make({ 100, 1 }));
	EXPECT_FALSE(a != Make({ 100, 1 }));
	EXPECT_TRUE(a != Make({ 1 }));
	EXPECT_TRUE(a != Make({ 1, 101 }));
	EXPECT_TRUE(a != Make({ 1, 100, 5000 }));
	EXPECT_TRUE(utl::SparseBitSet() == utl::SparseBitSet());
}

TEST(SparseBitSetTest, ContainAndOrdering)
{
	utl::SparseBitSet big = Make({ 1, 2, 100, 1000 });
	utl::SparseBitSet small = Make({ 2, 1000 });
	utl::SparseBitSet other = Make({ 2, 7 });	// big とも small とも包含関係にない

	EXPECT_TRUE(big.IsContain(small));
	EXPECT_FALSE(small.IsContain(big));
	EXPECT_TRUE(big.IsContain(utl::SparseBitSet()));
	EXPECT_TRUE(big.IsContain(big));
	EXPECT_FALSE(big.IsContain(other));

	EXPECT_TRUE(big >= small);
	EXPECT_TRUE(big > small);
	EXPECT_TRUE(small <= big);
	EXPECT_TRUE(small < big);
	EXPECT_FALSE(small >= big);
	EXPECT_FALSE(big <= small);

	EXPECT_TRUE(big >= big);
	EXPECT_TRUE(big <= big);
	EXPECT_FALSE(big > big);
	EXPECT_FALSE(big < big);

	// 比べられない組はどれも false
	EXPECT_FALSE(big >= other);
	EXPECT_FALSE(big <= other);
	EXPECT_FALSE(big > other);
	EXPECT_FALSE(big < other);
	EXPECT_FALSE(other >= big);
	EXPECT_FALSE(other <= big);
}

TEST(SparseBitSetTest, PlusMinusAssignNotAmbiguous)
{
	utl::SparseBitSet s;
	s += 3;
	s += 70;
	EXPECT_TRUE(s.Test(3));
	EXPECT_TRUE(s.Test(70));
	s -= 3;	// 0 などのリテラルがアロケータ側へ変換されないこと
	EXPECT_FALSE(s.Test(3));
	s += 0;
	EXPECT_TRUE(s.Test(0));
	s -= 0;
	EXPECT_FALSE(s.Test(0));
	s -= Make({ 70 });	// 集合同士は差集合
	EXPECT_TRUE(s.IsEmpty());
}

TEST(SparseBitSetTest, CopyAndMove)
{
	utl::SparseBitSet a = Make({ 1, 100, 100000 });
	utl::SparseBitSet copy(a);
	EXPECT_TRUE(copy == a);
	copy.Set(2);
	EXPECT_FALSE(a.Test(2));	// 独立している

	utl::SparseBitSet assigned;
	assigned.Set(9);
	assigned = a;
	EXPECT_TRUE(assigned == a);
	EXPECT_FALSE(assigned.Test(9));

	utl::SparseBitSet moved(utl::Move(copy));
	EXPECT_TRUE(moved == Make({ 1, 2, 100, 100000 }));
	EXPECT_TRUE(copy.IsEmpty());

	utl::SparseBitSet moveAssigned;
	moveAssigned = utl::Move(moved);
	EXPECT_TRUE(moveAssigned == Make({ 1, 2, 100, 100000 }));
	EXPECT_TRUE(moved.IsEmpty());
}

TEST(SparseBitSetTest, UsesGivenAllocator)
{
	CountingAllocator counter;
	utl::MemoryAllocatorDelegate delegate;
	delegate.Register<CountingAllocator, &CountingAllocator::Allocate, &CountingAllocator::Free>(&counter);
	{
		utl::SparseBitSet s{ utl::MemoryAllocatorRef(&delegate) };
		for (uint32_t i = 0; i < 200; ++i)
		{
			s.Set(i * 40);
		}
		EXPECT_GT(counter.allocCount, 0);
		EXPECT_TRUE(s.Test(40));

		utl::SparseBitSet t = Make({ 1 });
		t |= s;
		s -= t;
		s.Clear();

		utl::SparseBitSet copy(s);
		copy.Set(5);
		utl::SparseBitSet moved(utl::Move(copy));
		EXPECT_TRUE(moved.Test(5));
	}
	EXPECT_EQ(counter.allocCount, counter.freeCount);
}
