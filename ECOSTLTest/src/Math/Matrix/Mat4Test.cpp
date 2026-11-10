// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/Math/Matrix/Mat4.h"

#include <gtest/gtest.h>

#include <cmath>

#include "ECOSTL/Core/Math/Math.h"

namespace
{
	void ExpectNear(utl::Float3 actual, utl::Float3 expected, float eps = 1e-5f)
	{
		EXPECT_NEAR(actual.x, expected.x, eps);
		EXPECT_NEAR(actual.y, expected.y, eps);
		EXPECT_NEAR(actual.z, expected.z, eps);
	}

	// (x, y, z, 1) を射影まで含めて変換し、w で割った値を返す
	utl::Float3 Project(const utl::Mat4& m, utl::Float3 p)
	{
		float out[4];
		for (uint32_t r = 0; r < 4; ++r)
		{
			out[r] = m.At(r, 0) * p.x + m.At(r, 1) * p.y + m.At(r, 2) * p.z + m.At(r, 3);
		}
		return { out[0] / out[3], out[1] / out[3], out[2] / out[3] };
	}

	// 平行移動の行列(テスト用)
	constexpr utl::Mat4 Translation(utl::Float3 t)
	{
		utl::Mat4 r = utl::Mat4Identity();
		r.At(0, 3) = t.x;
		r.At(1, 3) = t.y;
		r.At(2, 3) = t.z;
		return r;
	}
}

TEST(Mat4, IdentityAndColumnMajorLayout) {
	constexpr utl::Mat4 i = utl::Mat4Identity();
	static_assert(i.At(0, 0) == 1.0f && i.At(3, 3) == 1.0f && i.At(0, 1) == 0.0f);
	// 列優先: 平行移動(4列目)は m[12..14]
	constexpr utl::Mat4 t = Translation({ 1.0f, 2.0f, 3.0f });
	static_assert(t.m[12] == 1.0f && t.m[13] == 2.0f && t.m[14] == 3.0f);
}

TEST(Mat4, ProductOrder) {
	constexpr utl::Mat4 t = Translation({ 1.0f, 2.0f, 3.0f });
	constexpr utl::Mat4 ti = utl::Mat4Identity() * t;
	static_assert(ti.At(0, 3) == 1.0f && ti.At(2, 3) == 3.0f);
	// a * b は b を掛けてから a: 拡大してから動かすのと、動かしてから拡大するのは違う
	utl::Mat4 s = utl::Mat4Identity();
	s.At(0, 0) = s.At(1, 1) = s.At(2, 2) = 2.0f;
	ExpectNear(utl::TransformPoint(t * s, { 1.0f, 1.0f, 1.0f }), { 3.0f, 4.0f, 5.0f });
	ExpectNear(utl::TransformPoint(s * t, { 1.0f, 1.0f, 1.0f }), { 4.0f, 6.0f, 8.0f });
}

TEST(Mat4, LookAtRHMovesEyeToOriginLookingMinusZ) {
	const utl::Float3 eye{ 3.0f, 4.0f, 5.0f };
	const utl::Float3 target{ 3.0f, 4.0f, 0.0f };
	const utl::Mat4 view = utl::Mat4LookAtRH(eye, target, { 0.0f, 1.0f, 0.0f });
	ExpectNear(utl::TransformPoint(view, eye), { 0.0f, 0.0f, 0.0f });
	// 見る点は -Z の先
	ExpectNear(utl::TransformPoint(view, target), { 0.0f, 0.0f, -5.0f });
	// 上は +Y、右は +X のまま(カメラは回っていない)
	ExpectNear(utl::TransformPoint(view, eye + utl::Float3{ 0.0f, 1.0f, 0.0f }), { 0.0f, 1.0f, 0.0f });
	ExpectNear(utl::TransformPoint(view, eye + utl::Float3{ 1.0f, 0.0f, 0.0f }), { 1.0f, 0.0f, 0.0f });
}

TEST(Mat4, PerspectiveRH_ZOMatchesTestGame) {
	// testGame の gameLib::Perspective と同じ要素
	const float fovY = utl::PI / 3.0f, aspect = 1.5f, zn = 0.1f, zf = 100.0f;
	const utl::Mat4 p = utl::Mat4PerspectiveRH_ZO(fovY, aspect, zn, zf);
	EXPECT_NEAR(p.At(0, 0), 1.1547005f, 1e-5f);
	EXPECT_NEAR(p.At(1, 1), 1.7320508f, 1e-5f);
	EXPECT_NEAR(p.At(2, 2), -1.001001f, 1e-5f);
	EXPECT_NEAR(p.At(2, 3), -0.1001001f, 1e-6f);
	EXPECT_EQ(p.At(3, 2), -1.0f);
	EXPECT_EQ(p.At(3, 3), 0.0f);
	EXPECT_EQ(p.At(0, 1), 0.0f);
}

TEST(Mat4, PerspectiveRH_ZODepthIsZeroToOne) {
	const float zn = 0.5f, zf = 50.0f;
	const utl::Mat4 p = utl::Mat4PerspectiveRH_ZO(utl::PI / 2.0f, 1.0f, zn, zf);
	EXPECT_NEAR(Project(p, { 0.0f, 0.0f, -zn }).z, 0.0f, 1e-5f);
	EXPECT_NEAR(Project(p, { 0.0f, 0.0f, -zf }).z, 1.0f, 1e-5f);
	// 画角の端(90° なら x = 距離)は x = 1
	EXPECT_NEAR(Project(p, { 10.0f, 0.0f, -10.0f }).x, 1.0f, 1e-5f);
}
