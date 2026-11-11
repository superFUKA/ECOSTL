// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/Math/Random/HashRandom.h"

#include <gtest/gtest.h>

#include <cstdint>

TEST(HashRandom, Random01MatchesTestGame) {
	// testGame の gameLib::Rand01 と同じ値になる
	static_assert(utl::Random01(0u) == 0.0f);
	EXPECT_FLOAT_EQ(utl::Random01(1u), 0.40834903717041016f);
	EXPECT_FLOAT_EQ(utl::Random01(12345u), 0.5671231746673584f);
	EXPECT_FLOAT_EQ(utl::RandomSigned(1u), 0.40834903717041016f * 2.0f - 1.0f);
}

TEST(HashRandom, RangesAndDeterminism) {
	double sum = 0.0;
	for (uint32_t seed = 0; seed < 10000; ++seed)
	{
		const float u = utl::Random01(seed);
		EXPECT_GE(u, 0.0f);
		EXPECT_LT(u, 1.0f);
		const float s = utl::RandomSigned(seed);
		EXPECT_GE(s, -1.0f);
		EXPECT_LT(s, 1.0f);
		EXPECT_EQ(u, utl::Random01(seed));
		sum += u;
	}
	// 一様なら平均はおよそ 0.5
	EXPECT_NEAR(sum / 10000.0, 0.5, 0.02);
}

TEST(HashRandom, RandomDirectionMatchesTestGame) {
	// testGame の gameLib::RandDirection と同じ値になる
	const utl::Float3 d5 = utl::RandomDirection(5u);
	EXPECT_NEAR(d5.x, 0.2110664f, 1e-5f);
	EXPECT_NEAR(d5.y, 0.1362959f, 1e-5f);
	EXPECT_NEAR(d5.z, 0.9679227f, 1e-5f);
	const utl::Float3 d1000 = utl::RandomDirection(1000u);
	EXPECT_NEAR(d1000.x, 0.6374646f, 1e-5f);
	EXPECT_NEAR(d1000.y, -0.6656585f, 1e-5f);
	EXPECT_NEAR(d1000.z, 0.3879919f, 1e-5f);
}

TEST(HashRandom, RandomDirectionIsUnitAndSpread) {
	utl::Float3 mean{ 0.0f, 0.0f, 0.0f };
	const uint32_t count = 10000;
	for (uint32_t seed = 0; seed < count; ++seed)
	{
		const utl::Float3 d = utl::RandomDirection(seed);
		EXPECT_NEAR(utl::Length(d), 1.0f, 1e-5f);
		mean += d;
	}
	// 球面で一様なら平均はおよそ原点
	mean *= 1.0f / static_cast<float>(count);
	EXPECT_LT(utl::Length(mean), 0.05f);
}
