// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Container/Set/Bit/BitSet.h"

#include <gtest/gtest.h>

TEST(BitSetTest, InitialEmpty)
{
	utl::BitSet<100> b;
	EXPECT_TRUE(b.IsEmpty());
	for (uint32_t i = 0; i < 100; i++)
	{
		EXPECT_FALSE(b.Test(i));
	}
}

TEST(BitSetTest, SetResetTest)
{
	utl::BitSet<100> b;
	b.Set(0);
	b.Set(63);
	b.Set(64);
	b.Set(99);
	EXPECT_FALSE(b.IsEmpty());
	EXPECT_TRUE(b.Test(0));
	EXPECT_TRUE(b.Test(63));
	EXPECT_TRUE(b.Test(64));
	EXPECT_TRUE(b.Test(99));
	EXPECT_FALSE(b.Test(1));
	EXPECT_FALSE(b.Test(65));
	b.Reset(63);
	EXPECT_FALSE(b.Test(63));
	EXPECT_TRUE(b.Test(64));
	b.Reset(63);	// 立っていないビットのリセットは何も起きない
	EXPECT_FALSE(b.Test(63));
}

TEST(BitSetTest, Clear)
{
	utl::BitSet<100> b;
	b.Set(5);
	b.Set(70);
	b.Clear();
	EXPECT_TRUE(b.IsEmpty());
}

TEST(BitSetTest, Sizes)
{
	EXPECT_EQ((utl::BitSet<64>::WordCount()), 1u);
	EXPECT_EQ((utl::BitSet<65>::WordCount()), 2u);
	EXPECT_EQ((utl::BitSet<1>::WordCount()), 1u);
	EXPECT_EQ((utl::BitSet<100, uint8_t>::WordCount()), 13u);
	utl::BitSet<100> b;
	EXPECT_EQ(b.GetSize(), 16u);
	utl::BitSet<100, uint8_t> b8;
	EXPECT_EQ(b8.GetSize(), 13u);
}

TEST(BitSetTest, GetWord)
{
	utl::BitSet<128, uint32_t> b;
	b.Set(0);
	b.Set(33);
	EXPECT_EQ(b.GetWord(0), 1u);
	EXPECT_EQ(b.GetWord(1), 2u);
	EXPECT_EQ(b.GetWord(2), 0u);
	EXPECT_EQ(b.GetData()[1], 2u);
}

TEST(BitSetTest, BitOperations)
{
	utl::BitSet<70> a;
	utl::BitSet<70> b;
	a.Set(1);
	a.Set(2);
	a.Set(66);
	b.Set(2);
	b.Set(3);
	b.Set(66);

	utl::BitSet<70> r = a;
	r.OrAssign(b);
	EXPECT_TRUE(r.Test(1) && r.Test(2) && r.Test(3) && r.Test(66));

	r = a;
	r.AndAssign(b);
	EXPECT_FALSE(r.Test(1));
	EXPECT_TRUE(r.Test(2));
	EXPECT_FALSE(r.Test(3));
	EXPECT_TRUE(r.Test(66));

	r = a;
	r.XorAssign(b);
	EXPECT_TRUE(r.Test(1));
	EXPECT_FALSE(r.Test(2));
	EXPECT_TRUE(r.Test(3));
	EXPECT_FALSE(r.Test(66));

	r = a;
	r.AndNotAssign(b);
	EXPECT_TRUE(r.Test(1));
	EXPECT_FALSE(r.Test(2));
	EXPECT_FALSE(r.Test(3));
	EXPECT_FALSE(r.Test(66));
}

TEST(BitSetTest, CompareAndCopy)
{
	utl::BitSet<70> a;
	utl::BitSet<70> b;
	EXPECT_TRUE(a == b);
	a.Set(69);
	EXPECT_TRUE(a != b);
	b = a;
	EXPECT_TRUE(a == b);
	utl::BitSet<70> c(a);
	EXPECT_TRUE(c == a);
	c.Reset(69);
	EXPECT_FALSE(c == a);
}

TEST(BitSetTest, IsContain)
{
	utl::BitSet<70> a;
	utl::BitSet<70> b;
	a.Set(1);
	a.Set(68);
	b.Set(68);
	EXPECT_TRUE(a.IsContain(b));
	EXPECT_FALSE(b.IsContain(a));
	utl::BitSet<70> empty;
	EXPECT_TRUE(a.IsContain(empty));
	EXPECT_TRUE(empty.IsContain(empty));
}

TEST(BitSetTest, Constexpr)
{
	constexpr auto make = []()
	{
		utl::BitSet<10> b;
		b.Set(3);
		return b;
	};
	constexpr auto b = make();
	static_assert(b.Test(3));
	static_assert(!b.Test(4));
	static_assert(!b.IsEmpty());
}
