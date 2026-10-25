// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Container/Array/DynamicArray.h"

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

TEST(DynamicArrayTest, CreateWithArgs)
{
	utl::DynamicArray<std::string> a(3, static_cast<utl::MemoryAllocatorDelegate*>(nullptr), 2, 'y');
	EXPECT_EQ(a.Size(), 3u);
	for (uint32_t i = 0; i < 3; i++)
	{
		EXPECT_EQ(a[i], "yy");
	}
	EXPECT_EQ(a.Get(3), nullptr);
	EXPECT_FALSE(a.IsValidIndex(3));
}

TEST(DynamicArrayTest, EmptyAndCreate)
{
	utl::DynamicArray<int> a;
	EXPECT_TRUE(a.IsEmpty());
	EXPECT_EQ(a.Data(), nullptr);
	EXPECT_TRUE(a.Create(4, 3));
	EXPECT_EQ(a.Size(), 4u);
	EXPECT_EQ(a[3], 3);
	EXPECT_TRUE(a.Create(0));
	EXPECT_TRUE(a.IsEmpty());
}

TEST(DynamicArrayTest, CopyMoveLifetime)
{
	Counted::alive = 0;
	{
		utl::DynamicArray<Counted> a(3, static_cast<utl::MemoryAllocatorDelegate*>(nullptr), 4);
		EXPECT_EQ(Counted::alive, 3);
		utl::DynamicArray<Counted> b(a);
		EXPECT_EQ(Counted::alive, 6);
		utl::DynamicArray<Counted> c(utl::Move(b));
		EXPECT_EQ(Counted::alive, 6);
		EXPECT_TRUE(b.IsEmpty());
		EXPECT_EQ(c[2].value, 4);
		a = c;
		c = utl::Move(a);
		EXPECT_EQ(Counted::alive, 3);
		a.Reset();
	}
	EXPECT_EQ(Counted::alive, 0);
}
