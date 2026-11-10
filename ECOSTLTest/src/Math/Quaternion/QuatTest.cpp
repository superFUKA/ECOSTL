// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/Math/Quaternion/Quat.h"

#include <gtest/gtest.h>

namespace
{
	constexpr utl::Float3 AXIS_X{ 1.0f, 0.0f, 0.0f };
	constexpr utl::Float3 AXIS_Y{ 0.0f, 1.0f, 0.0f };
	constexpr utl::Float3 AXIS_Z{ 0.0f, 0.0f, 1.0f };

	void ExpectNear(utl::Float3 actual, utl::Float3 expected, float eps = 1e-5f)
	{
		EXPECT_NEAR(actual.x, expected.x, eps);
		EXPECT_NEAR(actual.y, expected.y, eps);
		EXPECT_NEAR(actual.z, expected.z, eps);
	}

	// q と -q は同じ回転なので、符号をそろえて比べる
	void ExpectSameRotation(utl::Quat actual, utl::Quat expected, float eps = 1e-5f)
	{
		const float sgn = utl::QuatDot(actual, expected) < 0.0f ? -1.0f : 1.0f;
		EXPECT_NEAR(actual.x * sgn, expected.x, eps);
		EXPECT_NEAR(actual.y * sgn, expected.y, eps);
		EXPECT_NEAR(actual.z * sgn, expected.z, eps);
		EXPECT_NEAR(actual.w * sgn, expected.w, eps);
	}
}

TEST(Quat, ProductMatchesHamilton) {
	// ハミルトン積(testGame の gameLib::operator*(Quat, Quat) と同じ式)
	constexpr utl::Quat p = utl::Quat{ 1.0f, 2.0f, 3.0f, 4.0f } * utl::Quat{ 5.0f, 6.0f, 7.0f, 8.0f };
	static_assert(p.x == 24.0f && p.y == 48.0f && p.z == 48.0f && p.w == -6.0f);
	constexpr utl::Quat i = utl::QuatIdentity() * utl::Quat{ 1.0f, 2.0f, 3.0f, 4.0f };
	static_assert(i.x == 1.0f && i.y == 2.0f && i.z == 3.0f && i.w == 4.0f);
}

TEST(Quat, AxisAngleRotatesVector) {
	const utl::Quat q = utl::QuatAxisAngle(AXIS_Z, utl::PI * 0.5f);
	ExpectNear(utl::Rotate(q, AXIS_X), AXIS_Y);
	ExpectNear(utl::Rotate(q, AXIS_Y), -AXIS_X);
	ExpectNear(utl::Rotate(q, AXIS_Z), AXIS_Z);
	// 単位クォータニオンなら長さを変えない
	const utl::Float3 v{ 1.0f, -2.0f, 3.0f };
	EXPECT_NEAR(utl::Length(utl::Rotate(utl::QuatAxisAngle(utl::Normalize({ 1.0f, 1.0f, 0.0f }), 1.0f), v)), utl::Length(v), 1e-5f);
}

TEST(Quat, ProductComposesRotations) {
	// a * b は b で回してから a で回す
	const utl::Quat a = utl::QuatAxisAngle(AXIS_Y, utl::PI * 0.5f);
	const utl::Quat b = utl::QuatAxisAngle(AXIS_Z, utl::PI * 0.5f);
	const utl::Float3 v{ 1.0f, 2.0f, 3.0f };
	ExpectNear(utl::Rotate(a * b, v), utl::Rotate(a, utl::Rotate(b, v)));
	// 同じ軸なら角度が足される
	ExpectSameRotation(utl::QuatAxisAngle(AXIS_X, 0.3f) * utl::QuatAxisAngle(AXIS_X, 0.5f), utl::QuatAxisAngle(AXIS_X, 0.8f));
}

TEST(Quat, ConjugateIsInverse) {
	const utl::Quat q = utl::QuatAxisAngle(utl::Normalize({ 1.0f, 2.0f, 3.0f }), 0.7f);
	ExpectSameRotation(q * utl::QuatConjugate(q), utl::QuatIdentity());
	const utl::Float3 v{ -1.0f, 0.5f, 2.0f };
	ExpectNear(utl::Rotate(utl::QuatConjugate(q), utl::Rotate(q, v)), v);
}

TEST(Quat, NormalizeKeepsNearZero) {
	const utl::Quat n = utl::QuatNormalize({ 0.0f, 3.0f, 0.0f, 4.0f });
	EXPECT_NEAR(n.y, 0.6f, 1e-6f);
	EXPECT_NEAR(n.w, 0.8f, 1e-6f);
	const utl::Quat z = utl::QuatNormalize({ 0.0f, 0.0f, 0.0f, 0.0f });
	EXPECT_EQ(z.w, 0.0f);
}

TEST(Quat, LookRotationPointsPlusZAtForward) {
	// +Z を向くなら回転なし
	ExpectSameRotation(utl::QuatLookRotation(AXIS_Z), utl::QuatIdentity());
	// 任意の方向: +Z が forward に、+Y は上側に残る
	const utl::Float3 forward = utl::Normalize({ 1.0f, 0.5f, -2.0f });
	const utl::Quat q = utl::QuatLookRotation(forward * 3.0f);
	ExpectNear(utl::Rotate(q, AXIS_Z), forward);
	EXPECT_GT(utl::Rotate(q, AXIS_Y).y, 0.0f);
	EXPECT_NEAR(utl::Dot(utl::Rotate(q, AXIS_X), AXIS_Y), 0.0f, 1e-5f);
	// up を変えると +Y がその側に寄る
	const utl::Quat q2 = utl::QuatLookRotation(AXIS_X, AXIS_Z);
	ExpectNear(utl::Rotate(q2, AXIS_Z), AXIS_X);
	ExpectNear(utl::Rotate(q2, AXIS_Y), AXIS_Z);
}

TEST(Quat, LookRotationDegenerateCases) {
	// forward が 0 なら回転なし
	ExpectSameRotation(utl::QuatLookRotation({ 0.0f, 0.0f, 0.0f }), utl::QuatIdentity());
	// forward と up が平行(真上): testGame と同じく +X は X 軸のまま
	const utl::Quat up = utl::QuatLookRotation(AXIS_Y);
	ExpectNear(utl::Rotate(up, AXIS_Z), AXIS_Y);
	ExpectNear(utl::Rotate(up, AXIS_X), AXIS_X);
	// up が X 軸に平行なときも壊れない(X に寄せられないので Z 軸を使う)
	const utl::Quat side = utl::QuatLookRotation(AXIS_X, AXIS_X);
	ExpectNear(utl::Rotate(side, AXIS_Z), AXIS_X);
	EXPECT_NEAR(utl::Dot(utl::Rotate(side, AXIS_X), AXIS_X), 0.0f, 1e-5f);
}

TEST(Quat, NlerpEndsAndShortestPath) {
	const utl::Quat a = utl::QuatIdentity();
	const utl::Quat b = utl::QuatAxisAngle(AXIS_Y, 1.0f);
	ExpectSameRotation(utl::QuatNlerp(a, b, 0.0f), a);
	ExpectSameRotation(utl::QuatNlerp(a, b, 1.0f), b);
	// 符号を反転した b でも同じ途中の回転(近い側を通る)
	const utl::Quat negB{ -b.x, -b.y, -b.z, -b.w };
	ExpectSameRotation(utl::QuatNlerp(a, negB, 0.5f), utl::QuatNlerp(a, b, 0.5f));
	ExpectSameRotation(utl::QuatNlerp(a, b, 0.5f), utl::QuatAxisAngle(AXIS_Y, 0.5f), 1e-4f);
}
