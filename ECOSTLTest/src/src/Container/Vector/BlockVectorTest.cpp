// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Container/Vector/BlockVector.h"

#include <gtest/gtest.h>

#include <string>

TEST(BlockVectorTest, PushBackAcrossBlocks)
{
	utl::BlockVector<int> v(4);
	EXPECT_TRUE(v.IsEmpty());
	for (int i = 0; i < 10; i++)
	{
		v.PushBack(i);
	}
	EXPECT_EQ(v.Size(), 10u);
	EXPECT_EQ(v.GetBlockNum(), 3u);
	EXPECT_EQ(v.Capacity(), 12u);
	for (int i = 0; i < 10; i++)
	{
		EXPECT_EQ(v[i], i);
	}
	EXPECT_EQ(*v.Back(), 9);
	EXPECT_EQ(v.Get(10), nullptr);
	EXPECT_TRUE(v.IsValidIndex(9));
	EXPECT_FALSE(v.IsValidIndex(10));
}

TEST(BlockVectorTest, AddressStableOnGrowth)
{
	utl::BlockVector<int> v(2);
	int* first = v.PushBack(1);
	for (int i = 0; i < 100; i++)
	{
		v.PushBack(i);
	}
	EXPECT_EQ(first, v.Get(0));
}

TEST(BlockVectorTest, PopBackReleasesEmptyBlock)
{
	utl::BlockVector<int> v(2);
	v.PushBack(1);
	v.PushBack(2);
	v.PushBack(3);
	EXPECT_EQ(v.GetBlockNum(), 2u);
	v.PopBack();
	EXPECT_EQ(v.GetBlockNum(), 1u);
	EXPECT_EQ(v.Size(), 2u);
	v.PopBack();
	v.PopBack();
	EXPECT_TRUE(v.IsEmpty());
	v.PopBack();
	EXPECT_EQ(v.Back(), nullptr);
}

TEST(BlockVectorTest, ResizeGrowFillsPartialBlock)
{
	utl::BlockVector<int> v(4);
	v.PushBack(7);
	v.Resize(10, 5);
	EXPECT_EQ(v.Size(), 10u);
	EXPECT_EQ(v[0], 7);
	for (int i = 1; i < 10; i++)
	{
		EXPECT_EQ(v[i], 5);
	}
	v.Resize(3);
	EXPECT_EQ(v.Size(), 3u);
	EXPECT_EQ(v.GetBlockNum(), 1u);
	v.Resize(0);
	EXPECT_TRUE(v.IsEmpty());
}

TEST(BlockVectorTest, CopyAndMove)
{
	utl::BlockVector<std::string> a(2);
	for (int i = 0; i < 5; i++)
	{
		a.EmplaceBack(std::to_string(i));
	}
	utl::BlockVector<std::string> b(a);
	EXPECT_EQ(b.Size(), 5u);
	EXPECT_EQ(b[4], "4");
	b[0] = "x";
	EXPECT_EQ(a[0], "0");

	utl::BlockVector<std::string> c(Move(b));
	EXPECT_EQ(c.Size(), 5u);
	EXPECT_EQ(c[0], "x");
	EXPECT_TRUE(b.IsEmpty());

	utl::BlockVector<std::string> d(8);
	d = a;
	EXPECT_EQ(d.GetBlockSize(), 2u);
	EXPECT_EQ(d[3], "3");
	d = Move(c);
	EXPECT_EQ(d[0], "x");
}

TEST(BlockVectorTest, ConstAccess)
{
	utl::BlockVector<int> v(3, 10, utl::MemoryAllocatorRef());
	v.Resize(7, 1);
	const auto& cv = v;
	EXPECT_EQ(cv[6], 1);
	EXPECT_NE(cv.Begin(), nullptr);
	EXPECT_EQ(cv.BeginFromBlock(5), nullptr);
	EXPECT_EQ(*cv.Back(), 1);
}
