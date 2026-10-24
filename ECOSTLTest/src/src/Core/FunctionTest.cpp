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
		void Reset()
		{
			value = 0;
		}
		int value = 0;
	};

	/// <summary>
	/// ムーブ専用の型
	/// </summary>
	struct MoveOnly
	{
		explicit MoveOnly(int v) : value(v) {}
		MoveOnly(MoveOnly&& src) noexcept : value(src.value) { src.value = -1; }
		MoveOnly(const MoveOnly&) = delete;
		int value;
	};

	int TakeMoveOnly(MoveOnly m)
	{
		return m.value;
	}
	void Increment(int& v)
	{
		++v;
	}
}

TEST(FunctionTest, DefaultIsEmpty) {
	utl::Function<int(int, int)> f;
	EXPECT_FALSE(static_cast<bool>(f));
	utl::Function<int(int, int)> g(nullptr);
	EXPECT_FALSE(static_cast<bool>(g));
}

TEST(FunctionTest, CallFunction) {
	utl::Function<int(int, int)> f(Add);
	EXPECT_TRUE(static_cast<bool>(f));
	EXPECT_EQ(f(2, 3), 5);
}

TEST(FunctionTest, CallCaptureLessLambda) {
	utl::Function<int(int)> f([](int v) { return v * 2; });
	EXPECT_EQ(f(21), 42);
}

TEST(FunctionTest, CallVoid) {
	static int sum = 0;
	sum = 0;
	utl::Function<void(int)> f([](int v) { sum += v; });
	f(4);
	f(5);
	EXPECT_EQ(sum, 9);
}

TEST(FunctionTest, BindMethod) {
	Counter c;
	auto f = utl::Function<int(int)>::Bind<Counter, &Counter::Plus>(&c);
	EXPECT_TRUE(static_cast<bool>(f));
	EXPECT_EQ(f(3), 3);
	EXPECT_EQ(f(4), 7);
	EXPECT_EQ(c.value, 7);
}

TEST(FunctionTest, BindConstMethod) {
	Counter c;
	c.value = 8;
	const Counter& cc = c;
	auto f = utl::Function<int()>::Bind<Counter, &Counter::Get>(&cc);
	EXPECT_EQ(f(), 8);
}

TEST(FunctionTest, CopyAndAssign) {
	utl::Function<int(int, int)> f(Add);
	auto g = f;
	EXPECT_EQ(g(1, 2), 3);
	utl::Function<int(int, int)> h;
	h = g;
	EXPECT_EQ(h(4, 5), 9);
}

TEST(FunctionTest, ForwardsReferenceArgument) {
	utl::Function<void(int&)> f(Increment);
	int v = 1;
	f(v);
	f(v);
	EXPECT_EQ(v, 3);
}

TEST(FunctionTest, ForwardsMoveOnlyArgument) {
	utl::Function<int(MoveOnly)> f(TakeMoveOnly);
	EXPECT_EQ(f(MoveOnly(7)), 7);
}

TEST(FunctionTest, BindVoidMethod) {
	Counter c;
	c.value = 5;
	auto f = utl::Function<void()>::Bind<Counter, &Counter::Reset>(&c);
	f();
	EXPECT_EQ(c.value, 0);
}

TEST(FunctionTest, NoArgumentFunction) {
	utl::Function<int()> f([]() { return 11; });
	EXPECT_EQ(f(), 11);
}
