// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/Math/Simd/SimdLanes.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

TEST(SimdLanes, ForceSseOverridesCpu) {
	utl::simd::ForceSse(true);
	EXPECT_FALSE(utl::simd::UseAvx2());
	utl::simd::ForceSse(false);
	EXPECT_EQ(utl::simd::UseAvx2(), utl::simd::CpuHasAvx2());
}

#if UTL_SIMD_X86

namespace
{
	// 各レーンで行う計算(F4 と F8 で同じ式): a < b なら a*b+c、そうでなければ sqrt(|a|)。max/min で -10..10 に収める
	float ScalarKernel(float a, float b, float c)
	{
		const float v = (a < b) ? (a * b + c) : std::sqrt(std::fabs(a));
		return std::fmin(std::fmax(v, -10.0f), 10.0f);
	}

	const float A[8] = { 1.0f, -4.0f, 9.0f, 0.5f, -16.0f, 3.0f, 2.0f, -0.25f };
	const float B[8] = { 2.0f, -5.0f, 1.0f, 0.5f, 0.0f, 4.0f, -1.0f, 8.0f };
	const float C[8] = { 0.5f, 1.0f, -2.0f, 3.0f, 0.25f, -9.0f, 1.0f, 2.0f };
	// A < B のレーン: 0, 4, 5, 7
	constexpr uint32_t LESS_MASK = (1u << 0) | (1u << 4) | (1u << 5) | (1u << 7);
}

// ---------------- F4 ----------------

TEST(SimdLanes, F4ArithmeticAndSelect) {
	using utl::simd::F4;
	for (uint32_t base = 0; base < 8; base += 4)
	{
		const F4 a = utl::simd::Load(F4{}, A + base), b = utl::simd::Load(F4{}, B + base), c = utl::simd::Load(F4{}, C + base);
		const F4 lt = utl::simd::CmpLt(a, b);
		const F4 v = utl::simd::Select(lt, utl::simd::Fma(a, b, c), utl::simd::Sqrt(utl::simd::Abs(a)));
		const F4 r = utl::simd::Min(utl::simd::Max(v, utl::simd::Set1(F4{}, -10.0f)), utl::simd::Set1(F4{}, 10.0f));
		float out[4];
		utl::simd::Store(r, out);
		for (uint32_t k = 0; k < 4; ++k)
		{
			EXPECT_FLOAT_EQ(out[k], ScalarKernel(A[base + k], B[base + k], C[base + k])) << "lane " << base + k;
		}
		EXPECT_EQ(utl::simd::MoveMask(lt), (LESS_MASK >> base) & 0xFu);
	}
}

TEST(SimdLanes, F4BasicOps) {
	using utl::simd::F4;
	const F4 a = utl::simd::Load(F4{}, A), b = utl::simd::Load(F4{}, B);
	float out[4];
	utl::simd::StoreLanes((a + b) - (a * b) / utl::simd::Set1(F4{}, 2.0f), out);
	for (uint32_t k = 0; k < 4; ++k)
	{
		EXPECT_FLOAT_EQ(out[k], (A[k] + B[k]) - (A[k] * B[k]) / 2.0f);
	}
	utl::simd::Store(utl::simd::Zero(F4{}), out);
	EXPECT_EQ(out[0], 0.0f);
	EXPECT_EQ(out[3], 0.0f);
	// 比較の組み合わせ: a <= b かつ a > -5 → レーン 0, 3
	const F4 m = utl::simd::And(utl::simd::CmpLe(a, b), utl::simd::CmpGt(a, utl::simd::Set1(F4{}, -5.0f)));
	EXPECT_EQ(utl::simd::MoveMask(m), (1u << 0) | (1u << 3));
	EXPECT_EQ(utl::simd::MoveMask(utl::simd::Or(m, utl::simd::CmpGt(a, b))), (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3));
	EXPECT_EQ(utl::simd::MoveMask(utl::simd::AndNot(m, utl::simd::CmpLe(a, b))), 0u);
}

TEST(SimdLanes, F4Load3SplitsStructOfThree) {
	using utl::simd::F4;
	float xyz[12];
	for (uint32_t i = 0; i < 12; ++i)
	{
		xyz[i] = static_cast<float>(i);
	}
	F4 x, y, z;
	utl::simd::Load3(F4{}, xyz, x, y, z);
	float ox[4], oy[4], oz[4];
	utl::simd::Store(x, ox);
	utl::simd::Store(y, oy);
	utl::simd::Store(z, oz);
	for (uint32_t k = 0; k < 4; ++k)
	{
		EXPECT_EQ(ox[k], static_cast<float>(k * 3));
		EXPECT_EQ(oy[k], static_cast<float>(k * 3 + 1));
		EXPECT_EQ(oz[k], static_cast<float>(k * 3 + 2));
	}
}

TEST(SimdLanes, F4Rows4RoundTrip) {
	using utl::simd::F4;
	float rows[16];
	for (uint32_t i = 0; i < 16; ++i)
	{
		rows[i] = static_cast<float>(i);
	}
	F4 a, b, c, d;
	utl::simd::LoadRows4(F4{}, rows, a, b, c, d);
	float oa[4];
	utl::simd::Store(b, oa);
	// b はそれぞれの要素の2番目の成分
	EXPECT_EQ(oa[0], 1.0f);
	EXPECT_EQ(oa[3], 13.0f);
	// 間隔 5 で書き戻す(5 番目の float は触らない)
	float dst[20];
	for (float& v : dst)
	{
		v = -1.0f;
	}
	utl::simd::StoreRows4(dst, 5, a, b, c, d);
	for (uint32_t k = 0; k < 4; ++k)
	{
		for (uint32_t j = 0; j < 4; ++j)
		{
			EXPECT_EQ(dst[k * 5 + j], rows[k * 4 + j]);
		}
		EXPECT_EQ(dst[k * 5 + 4], -1.0f);
	}
}

// ---------------- F8 ----------------

UTL_SIMD_AVX2_BEGIN
namespace
{
	// F8 のカーネル。AVX2 の区間の中で、ポインタで受け渡す
	void F8Kernel(const float* a, const float* b, const float* c, float* out, uint32_t* mask)
	{
		using utl::simd::F8;
		const F8 va = utl::simd::Load(F8{}, a), vb = utl::simd::Load(F8{}, b), vc = utl::simd::Load(F8{}, c);
		const F8 lt = utl::simd::CmpLt(va, vb);
		const F8 v = utl::simd::Select(lt, utl::simd::Fma(va, vb, vc), utl::simd::Sqrt(utl::simd::Abs(va)));
		const F8 r = utl::simd::Min(utl::simd::Max(v, utl::simd::Set1(F8{}, -10.0f)), utl::simd::Set1(F8{}, 10.0f));
		utl::simd::Store(r, out);
		*mask = utl::simd::MoveMask(lt);
	}

	void F8Load3(const float* xyz, float* ox, float* oy, float* oz)
	{
		using utl::simd::F8;
		F8 x, y, z;
		utl::simd::Load3(F8{}, xyz, x, y, z);
		utl::simd::StoreLanes(x, ox);
		utl::simd::StoreLanes(y, oy);
		utl::simd::StoreLanes(z, oz);
	}

	void F8Rows4RoundTrip(const float* rows, float* dst, int stride, float* secondComponent)
	{
		using utl::simd::F8;
		F8 a, b, c, d;
		utl::simd::LoadRows4(F8{}, rows, a, b, c, d);
		utl::simd::Store(b, secondComponent);
		utl::simd::StoreRows4(dst, stride, a, b, c, d);
	}
}
UTL_SIMD_AVX2_END

TEST(SimdLanes, F8MatchesScalar) {
	if (!utl::simd::CpuHasAvx2())
	{
		GTEST_SKIP() << "この CPU には AVX2 が無い";
	}
	float out[8];
	uint32_t mask = 0;
	F8Kernel(A, B, C, out, &mask);
	for (uint32_t k = 0; k < 8; ++k)
	{
		// F8 の Fma は丸めが1回なので、最後のビットが違ってよい
		EXPECT_NEAR(out[k], ScalarKernel(A[k], B[k], C[k]), 1e-6f) << "lane " << k;
	}
	EXPECT_EQ(mask, LESS_MASK);
}

TEST(SimdLanes, F8Load3Gathers) {
	if (!utl::simd::CpuHasAvx2())
	{
		GTEST_SKIP() << "この CPU には AVX2 が無い";
	}
	float xyz[24];
	for (uint32_t i = 0; i < 24; ++i)
	{
		xyz[i] = static_cast<float>(i);
	}
	float ox[8], oy[8], oz[8];
	F8Load3(xyz, ox, oy, oz);
	for (uint32_t k = 0; k < 8; ++k)
	{
		EXPECT_EQ(ox[k], static_cast<float>(k * 3));
		EXPECT_EQ(oy[k], static_cast<float>(k * 3 + 1));
		EXPECT_EQ(oz[k], static_cast<float>(k * 3 + 2));
	}
}

TEST(SimdLanes, F8Rows4RoundTrip) {
	if (!utl::simd::CpuHasAvx2())
	{
		GTEST_SKIP() << "この CPU には AVX2 が無い";
	}
	float rows[32];
	for (uint32_t i = 0; i < 32; ++i)
	{
		rows[i] = static_cast<float>(i);
	}
	float dst[40];
	for (float& v : dst)
	{
		v = -1.0f;
	}
	float second[8];
	F8Rows4RoundTrip(rows, dst, 5, second);
	for (uint32_t k = 0; k < 8; ++k)
	{
		EXPECT_EQ(second[k], static_cast<float>(k * 4 + 1));
		for (uint32_t j = 0; j < 4; ++j)
		{
			EXPECT_EQ(dst[k * 5 + j], rows[k * 4 + j]);
		}
		EXPECT_EQ(dst[k * 5 + 4], -1.0f);
	}
}

#endif
