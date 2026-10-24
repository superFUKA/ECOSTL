// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Core/Function.h"

#include <gtest/gtest.h>

namespace
{
	int Add(int a, int b)
	{
		return a + b;
	}

	class Counter
	{
	public:
		int Plus(int v)
		{
			value += v;
			return value;
		}
		int Get() const
		{
			return value;
		}
		int value = 0;
	};
}

TEST(FunctionTest, DefaultIsEmpty) {
	utl::FunctionObject<int(int, int)> f;
	EXPECT_FALSE(static_cast<bool>(f));
	utl::FunctionObject<int(int, int)> g(nullptr);
	EXPECT_FALSE(static_cast<bool>(g));
}

TEST(FunctionTest, CallFunction) {
	utl::FunctionObject<int(int, int)> f(Add);
	EXPECT_TRUE(static_cast<bool>(f));
	EXPECT_EQ(f(2, 3), 5);
}

TEST(FunctionTest, CallCaptureLessLambda) {
	utl::FunctionObject<int(int)> f([](int v) { return v * 2; });
	EXPECT_EQ(f(21), 42);
}

TEST(FunctionTest, CallVoid) {
	static int sum = 0;
	sum = 0;
	utl::FunctionObject<void(int)> f([](int v) { sum += v; });
	f(4);
	f(5);
	EXPECT_EQ(sum, 9);
}

TEST(FunctionTest, BindMethod) {
	Counter c;
	auto f = utl::FunctionObject<int(int)>::Bind<Counter, &Counter::Plus>(&c);
	EXPECT_TRUE(static_cast<bool>(f));
	EXPECT_EQ(f(3), 3);
	EXPECT_EQ(f(4), 7);
	EXPECT_EQ(c.value, 7);
}

TEST(FunctionTest, BindConstMethod) {
	Counter c;
	c.value = 8;
	const Counter& cc = c;
	auto f = utl::FunctionObject<int()>::Bind<Counter, &Counter::Get>(&cc);
	EXPECT_EQ(f(), 8);
}

TEST(FunctionTest, CopyAndAssign) {
	utl::FunctionObject<int(int, int)> f(Add);
	auto g = f;
	EXPECT_EQ(g(1, 2), 3);
	utl::FunctionObject<int(int, int)> h;
	h = g;
	EXPECT_EQ(h(4, 5), 9);
}
