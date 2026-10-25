// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Container/Array/StaticArray.h"

#include <gtest/gtest.h>

#include <string>

namespace
{
	struct Counted
	{
		static inline int alive = 0;
		Counted(int v = 0) : value(v) { ++alive; }
		Counted(const Counted& s) : value(s.value) { ++alive; }
		Counted(Counted&& s)noexcept : value(s.value) { ++alive; }
		Counted& operator=(const Counted&) = default;
		~Counted() { --alive; }
		int value;
	};
}

TEST(StaticArrayTest, DefaultAndArgs)
{
	utl::StaticArray<int, 4> a;
	EXPECT_EQ(a.Size(), 4u);
	EXPECT_EQ(a[3], 0);

	utl::StaticArray<std::string, 3> s(2, 'x');
	for (uint32_t i = 0; i < s.Size(); i++)
	{
		EXPECT_EQ(s[i], "xx");
	}
}

TEST(StaticArrayTest, AccessAndRange)
{
	utl::StaticArray<int, 3> a(7);
	a[1] = 9;
	EXPECT_EQ(*a.Get(1), 9);
	EXPECT_EQ(a.Get(3), nullptr);
	EXPECT_TRUE(a.IsValidIndex(2));
	EXPECT_FALSE(a.IsValidIndex(3));
	EXPECT_EQ(a.End() - a.Begin(), 3);
	EXPECT_EQ(a.Data(), a.Begin());
}

TEST(StaticArrayTest, CopyMoveLifetime)
{
	Counted::alive = 0;
	{
		utl::StaticArray<Counted, 3> a(5);
		EXPECT_EQ(Counted::alive, 3);
		utl::StaticArray<Counted, 3> b(a);
		EXPECT_EQ(Counted::alive, 6);
		EXPECT_EQ(b[2].value, 5);
		utl::StaticArray<Counted, 3> c(utl::Move(b));
		EXPECT_EQ(c[0].value, 5);
		a[0].value = 1;
		c = a;
		EXPECT_EQ(c[0].value, 1);
	}
	EXPECT_EQ(Counted::alive, 0);
}
