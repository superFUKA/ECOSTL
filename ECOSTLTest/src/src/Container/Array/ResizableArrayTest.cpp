// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Container/Array/ResizableArray.h"

#include <gtest/gtest.h>

#include <string>

namespace
{
	struct Counted
	{
		static inline int alive = 0;
		Counted(int v = 0) : value(v) { ++alive; }
		Counted(const Counted& s) : value(s.value) { ++alive; }
		~Counted() { --alive; }
		int value;
	};
}

TEST(ResizableArrayTest, ResizeGrowShrink)
{
	utl::ResizableArray<int> a;
	EXPECT_TRUE(a.IsEmpty());
	EXPECT_TRUE(a.Resize(3, 5));
	EXPECT_EQ(a.Size(), 3u);
	a[0] = 1;
	EXPECT_TRUE(a.Resize(5, 8));
	EXPECT_EQ(a[0], 1);
	EXPECT_EQ(a[2], 5);
	EXPECT_EQ(a[4], 8);
	EXPECT_TRUE(a.Resize(2));
	EXPECT_EQ(a.Size(), 2u);
	EXPECT_EQ(a[1], 5);
	EXPECT_FALSE(a.Resize(2));
	EXPECT_TRUE(a.Resize(0));
	EXPECT_TRUE(a.IsEmpty());
	EXPECT_EQ(a.Data(), nullptr);
}

TEST(ResizableArrayTest, CreateWithArgs)
{
	utl::ResizableArray<std::string> a(2, static_cast<utl::MemoryAllocatorDelegate*>(nullptr), 3, 'z');
	EXPECT_EQ(a.Size(), 2u);
	EXPECT_EQ(a[1], "zzz");
	a.Resize(4, "q");
	EXPECT_EQ(a[0], "zzz");
	EXPECT_EQ(a[3], "q");
	EXPECT_EQ(a.Get(4), nullptr);
}

TEST(ResizableArrayTest, CopyDoesNotModifySource)
{
	utl::ResizableArray<std::string> a(2, static_cast<utl::MemoryAllocatorDelegate*>(nullptr), "abc");
	utl::ResizableArray<std::string> b(a);
	EXPECT_EQ(a[0], "abc");
	EXPECT_EQ(b[1], "abc");
	b[0] = "x";
	EXPECT_EQ(a[0], "abc");
	utl::ResizableArray<std::string> c;
	c = a;
	EXPECT_EQ(c[1], "abc");
	utl::ResizableArray<std::string> d(utl::Move(c));
	EXPECT_TRUE(c.IsEmpty());
	EXPECT_EQ(d.Size(), 2u);
}

TEST(ResizableArrayTest, Lifetime)
{
	Counted::alive = 0;
	{
		utl::ResizableArray<Counted> a(3, static_cast<utl::MemoryAllocatorDelegate*>(nullptr), 1);
		EXPECT_EQ(Counted::alive, 3);
		a.Resize(6, 2);
		EXPECT_EQ(Counted::alive, 6);
		a.Resize(1);
		EXPECT_EQ(Counted::alive, 1);
		utl::ResizableArray<Counted> b(a);
		a = b;
		EXPECT_EQ(Counted::alive, 2);
	}
	EXPECT_EQ(Counted::alive, 0);
}
