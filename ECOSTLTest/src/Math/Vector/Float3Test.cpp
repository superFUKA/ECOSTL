// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/Math/Vector/Float3.h"

#include <gtest/gtest.h>

#include <cmath>

namespace
{
	void ExpectNear(utl::Float3 actual, utl::Float3 expected, float eps = 1e-5f)
	{
		EXPECT_NEAR(actual.x, expected.x, eps);
		EXPECT_NEAR(actual.y, expected.y, eps);
		EXPECT_NEAR(actual.z, expected.z, eps);
	}
}

TEST(Float3, ArithmeticIsConstexpr) {
	constexpr utl::Float3 a{ 1.0f, 2.0f, 3.0f };
	constexpr utl::Float3 b{ 4.0f, -5.0f, 6.0f };
	constexpr utl::Float3 sum = a + b;
	static_assert(sum.x == 5.0f && sum.y == -3.0f && sum.z == 9.0f);
	constexpr utl::Float3 diff = a - b;
	static_assert(diff.x == -3.0f && diff.y == 7.0f && diff.z == -3.0f);
	constexpr utl::Float3 scaled = 2.0f * a;
	static_assert(scaled.x == 2.0f && scaled.y == 4.0f && scaled.z == 6.0f);
	constexpr utl::Float3 neg = -a;
	static_assert(neg.x == -1.0f && neg.y == -2.0f && neg.z == -3.0f);
	static_assert(utl::Dot(a, b) == 4.0f - 10.0f + 18.0f);
	static_assert(utl::LengthSq(a) == 14.0f);
}

TEST(Float3, CompoundAssignment) {
	utl::Float3 v{ 1.0f, 1.0f, 1.0f };
	v += { 1.0f, 2.0f, 3.0f };
	v -= { 0.5f, 0.5f, 0.5f };
	v *= 2.0f;
	ExpectNear(v, { 3.0f, 5.0f, 7.0f });
}

TEST(Float3, CrossIsRightHanded) {
	constexpr utl::Float3 x{ 1.0f, 0.0f, 0.0f };
	constexpr utl::Float3 y{ 0.0f, 1.0f, 0.0f };
	constexpr utl::Float3 z = utl::Cross(x, y);
	static_assert(z.x == 0.0f && z.y == 0.0f && z.z == 1.0f);
	// 外積はどちらにも垂直
	const utl::Float3 a{ 1.0f, 2.0f, 3.0f }, b{ -2.0f, 0.5f, 4.0f };
	const utl::Float3 c = utl::Cross(a, b);
	EXPECT_NEAR(utl::Dot(c, a), 0.0f, 1e-5f);
	EXPECT_NEAR(utl::Dot(c, b), 0.0f, 1e-5f);
}

TEST(Float3, LengthAndNormalize) {
	EXPECT_FLOAT_EQ(utl::Length({ 3.0f, 4.0f, 0.0f }), 5.0f);
	ExpectNear(utl::Normalize({ 0.0f, 0.0f, 5.0f }), { 0.0f, 0.0f, 1.0f });
	EXPECT_NEAR(utl::Length(utl::Normalize({ 1.0f, -2.0f, 3.0f })), 1.0f, 1e-6f);
}

TEST(Float3, NormalizeZeroGivesZeroNotNaN) {
	const utl::Float3 n = utl::Normalize({ 0.0f, 0.0f, 0.0f });
	EXPECT_FALSE(std::isnan(n.x));
	ExpectNear(n, { 0.0f, 0.0f, 0.0f }, 0.0f);
}
