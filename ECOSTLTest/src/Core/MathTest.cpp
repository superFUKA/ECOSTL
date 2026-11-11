// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/Core/Math/Math.h"

#include <gtest/gtest.h>

#include <cstdint>

TEST(Math, MinMaxAreConstexpr) {
	static_assert(utl::Min(3, 5) == 3);
	static_assert(utl::Max(3, 5) == 5);
	static_assert(utl::Min(-1.5f, 2.0f) == -1.5f);
	EXPECT_EQ(utl::Max(7u, 7u), 7u);
}

TEST(Math, AbsAndClamp) {
	static_assert(utl::Abs(-4) == 4);
	static_assert(utl::Abs(4) == 4);
	EXPECT_FLOAT_EQ(utl::Abs(-2.5f), 2.5f);
	static_assert(utl::Clamp(5, 0, 3) == 3);
	static_assert(utl::Clamp(-1, 0, 3) == 0);
	static_assert(utl::Clamp(2, 0, 3) == 2);
	EXPECT_FLOAT_EQ(utl::Saturate(1.5f), 1.0f);
	EXPECT_FLOAT_EQ(utl::Saturate(-0.5f), 0.0f);
	EXPECT_FLOAT_EQ(utl::Saturate(0.25f), 0.25f);
}

TEST(Math, LerpAndSmoothStep) {
	EXPECT_FLOAT_EQ(utl::Lerp(2.0f, 6.0f, 0.0f), 2.0f);
	EXPECT_FLOAT_EQ(utl::Lerp(2.0f, 6.0f, 1.0f), 6.0f);
	EXPECT_FLOAT_EQ(utl::Lerp(2.0f, 6.0f, 0.25f), 3.0f);
	// 範囲の外は 0 と 1、真ん中は 0.5、端の傾きは 0(エルミート)
	EXPECT_FLOAT_EQ(utl::SmoothStep(1.0f, 3.0f, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(utl::SmoothStep(1.0f, 3.0f, 4.0f), 1.0f);
	EXPECT_FLOAT_EQ(utl::SmoothStep(1.0f, 3.0f, 2.0f), 0.5f);
	EXPECT_FLOAT_EQ(utl::SmoothStep(0.0f, 1.0f, 0.25f), 0.25f * 0.25f * (3.0f - 0.5f));
	EXPECT_DOUBLE_EQ(utl::SmoothStep(0.0, 1.0, 0.5), 0.5);
}

TEST(Math, Constants) {
	EXPECT_FLOAT_EQ(utl::TAU, 2.0f * utl::PI);
	EXPECT_NEAR(utl::PI, 3.14159265f, 1e-6f);
}

TEST(Math, RotateBits) {
	static_assert(utl::RotL(0x80000001u, 1) == 0x00000003u);
	static_assert(utl::RotR(0x80000001u, 1) == 0xC0000000u);
	// 回す量はビット数で剰余を取る
	static_assert(utl::RotL(0x12345678u, 32) == 0x12345678u);
	static_assert(utl::RotL(0x12345678u, 36) == utl::RotL(0x12345678u, 4));
	// 8bit・64bit でも回せる(整数昇格で上位に出たビットは捨てる)
	static_assert(utl::RotL(static_cast<uint8_t>(0x81), 1) == static_cast<uint8_t>(0x03));
	static_assert(utl::RotR(static_cast<uint8_t>(0x81), 1) == static_cast<uint8_t>(0xC0));
	static_assert(utl::RotL(0x8000000000000001ull, 4) == 0x18ull);
	EXPECT_EQ(utl::RotR(utl::RotL(0xDEADBEEFu, 13), 13), 0xDEADBEEFu);
}
