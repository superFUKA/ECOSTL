// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Core/Utility.h"

#include <gtest/gtest.h>

namespace
{
	/// <summary>
	/// コピーとムーブの回数を数えるテスト用の型
	/// </summary>
	struct Counted
	{
		static inline int copied = 0;
		static inline int moved = 0;

		static void ResetCount()
		{
			copied = 0;
			moved = 0;
		}

		Counted(int v = 0)
			: value(v)
		{}
		Counted(const Counted& src)
			: value(src.value)
		{
			++copied;
		}
		Counted(Counted&& src) noexcept
			: value(src.value)
		{
			src.value = -1;
			++moved;
		}
		Counted& operator=(const Counted& src)
		{
			value = src.value;
			++copied;
			return *this;
		}
		Counted& operator=(Counted&& src) noexcept
		{
			value = src.value;
			src.value = -1;
			++moved;
			return *this;
		}

		int value;
	};

	/// <summary>
	/// ムーブで例外を投げるかもしれない型
	/// </summary>
	struct ThrowingMove
	{
		ThrowingMove() = default;
		ThrowingMove(ThrowingMove&&) {}
		ThrowingMove& operator=(ThrowingMove&&)
		{
			return *this;
		}
	};

	// 受け取った値の種類を返す
	constexpr int Kind(int&)
	{
		return 1;
	}
	constexpr int Kind(const int&)
	{
		return 2;
	}
	constexpr int Kind(int&&)
	{
		return 3;
	}

	template<typename T>
	constexpr int ForwardKind(T&& value)
	{
		return Kind(utl::Forward<T>(value));
	}

	constexpr int SwapInConstexpr()
	{
		int a = 1;
		int b = 2;
		utl::Swap(a, b);
		return a * 10 + b;
	}
}

//----------------------------------------
// Move
//----------------------------------------

static_assert(utl::IsSameV<decltype(utl::Move(utl::DeclVal<int&>())), int&&>);
static_assert(utl::IsSameV<decltype(utl::Move(utl::DeclVal<const int&>())), const int&&>);
static_assert(noexcept(utl::Move(utl::DeclVal<int&>())));

TEST(Utility, MoveCallsMoveConstructor) {
	Counted::ResetCount();
	Counted src(5);

	Counted dst(utl::Move(src));

	EXPECT_EQ(dst.value, 5);
	EXPECT_EQ(src.value, -1);
	EXPECT_EQ(Counted::moved, 1);
	EXPECT_EQ(Counted::copied, 0);
}

//----------------------------------------
// Forward
//----------------------------------------

TEST(Utility, ForwardKeepsValueCategory) {
	int value = 0;
	const int constValue = 0;

	EXPECT_EQ(ForwardKind(value), 1);
	EXPECT_EQ(ForwardKind(constValue), 2);
	EXPECT_EQ(ForwardKind(0), 3);
	EXPECT_EQ(ForwardKind(utl::Move(value)), 3);
}

static_assert(ForwardKind(0) == 3);

//----------------------------------------
// Swap
//----------------------------------------

TEST(Utility, SwapExchangesValues) {
	int a = 1;
	int b = 2;

	utl::Swap(a, b);

	EXPECT_EQ(a, 2);
	EXPECT_EQ(b, 1);
}

TEST(Utility, SwapUsesMoveOnly) {
	Counted a(1);
	Counted b(2);
	Counted::ResetCount();

	utl::Swap(a, b);

	EXPECT_EQ(a.value, 2);
	EXPECT_EQ(b.value, 1);
	EXPECT_EQ(Counted::copied, 0);
	EXPECT_EQ(Counted::moved, 3);
}

static_assert(SwapInConstexpr() == 21);
static_assert(noexcept(utl::Swap(utl::DeclVal<int&>(), utl::DeclVal<int&>())));
static_assert(noexcept(utl::Swap(utl::DeclVal<Counted&>(), utl::DeclVal<Counted&>())));
static_assert(!noexcept(utl::Swap(utl::DeclVal<ThrowingMove&>(), utl::DeclVal<ThrowingMove&>())));
