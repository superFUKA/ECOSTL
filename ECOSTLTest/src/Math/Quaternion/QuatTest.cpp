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
		const float sgn = utl::Dot(actual, expected) < 0.0f ? -1.0f : 1.0f;
		EXPECT_NEAR(actual.x * sgn, expected.x, eps);
		EXPECT_NEAR(actual.y * sgn, expected.y, eps);
		EXPECT_NEAR(actual.z * sgn, expected.z, eps);
		EXPECT_NEAR(actual.w * sgn, expected.w, eps);
	}
}

TEST(Quat, IsPlainFourFloats) {
	// static 関数を持っても POD のまま
	static_assert(sizeof(utl::Quat) == sizeof(float) * 4);
	static_assert(utl::IsTriviallyCopyableV<utl::Quat>);
}

TEST(Quat, ProductMatchesHamilton) {
	// ハミルトン積(testGame の gameLib::operator*(Quat, Quat) と同じ式)
	constexpr utl::Quat p = utl::Quat{ 1.0f, 2.0f, 3.0f, 4.0f } * utl::Quat{ 5.0f, 6.0f, 7.0f, 8.0f };
	static_assert(p.x == 24.0f && p.y == 48.0f && p.z == 48.0f && p.w == -6.0f);
	constexpr utl::Quat i = utl::Quat::Identity() * utl::Quat{ 1.0f, 2.0f, 3.0f, 4.0f };
	static_assert(i.x == 1.0f && i.y == 2.0f && i.z == 3.0f && i.w == 4.0f);
}

TEST(Quat, AxisAngleRotatesVector) {
	const utl::Quat q = utl::Quat::FromAxisAngle(AXIS_Z, utl::PI * 0.5f);
	ExpectNear(utl::Rotate(q, AXIS_X), AXIS_Y);
	ExpectNear(utl::Rotate(q, AXIS_Y), -AXIS_X);
	ExpectNear(utl::Rotate(q, AXIS_Z), AXIS_Z);
	// 単位クォータニオンなら長さを変えない
	const utl::Float3 v{ 1.0f, -2.0f, 3.0f };
	EXPECT_NEAR(utl::Length(utl::Rotate(utl::Quat::FromAxisAngle(utl::Normalize(utl::Float3{ 1.0f, 1.0f, 0.0f }), 1.0f), v)), utl::Length(v), 1e-5f);
}

TEST(Quat, ProductComposesRotations) {
	// a * b は b で回してから a で回す
	const utl::Quat a = utl::Quat::FromAxisAngle(AXIS_Y, utl::PI * 0.5f);
	const utl::Quat b = utl::Quat::FromAxisAngle(AXIS_Z, utl::PI * 0.5f);
	const utl::Float3 v{ 1.0f, 2.0f, 3.0f };
	ExpectNear(utl::Rotate(a * b, v), utl::Rotate(a, utl::Rotate(b, v)));
	// 同じ軸なら角度が足される
	ExpectSameRotation(utl::Quat::FromAxisAngle(AXIS_X, 0.3f) * utl::Quat::FromAxisAngle(AXIS_X, 0.5f), utl::Quat::FromAxisAngle(AXIS_X, 0.8f));
}

TEST(Quat, ConjugateIsInverse) {
	const utl::Quat q = utl::Quat::FromAxisAngle(utl::Normalize(utl::Float3{ 1.0f, 2.0f, 3.0f }), 0.7f);
	ExpectSameRotation(q * utl::Conjugate(q), utl::Quat::Identity());
	const utl::Float3 v{ -1.0f, 0.5f, 2.0f };
	ExpectNear(utl::Rotate(utl::Conjugate(q), utl::Rotate(q, v)), v);
}

TEST(Quat, NormalizeKeepsNearZero) {
	const utl::Quat n = utl::Normalize(utl::Quat{ 0.0f, 3.0f, 0.0f, 4.0f });
	EXPECT_NEAR(n.y, 0.6f, 1e-6f);
	EXPECT_NEAR(n.w, 0.8f, 1e-6f);
	const utl::Quat z = utl::Normalize(utl::Quat{ 0.0f, 0.0f, 0.0f, 0.0f });
	EXPECT_EQ(z.w, 0.0f);
}

TEST(Quat, LookRotationPointsPlusZAtForward) {
	// +Z を向くなら回転なし
	ExpectSameRotation(utl::Quat::LookRotation(AXIS_Z), utl::Quat::Identity());
	// 任意の方向: +Z が forward に、+Y は上側に残る
	const utl::Float3 forward = utl::Normalize(utl::Float3{ 1.0f, 0.5f, -2.0f });
	const utl::Quat q = utl::Quat::LookRotation(forward * 3.0f);
	ExpectNear(utl::Rotate(q, AXIS_Z), forward);
	EXPECT_GT(utl::Rotate(q, AXIS_Y).y, 0.0f);
	EXPECT_NEAR(utl::Dot(utl::Rotate(q, AXIS_X), AXIS_Y), 0.0f, 1e-5f);
	// up を変えると +Y がその側に寄る
	const utl::Quat q2 = utl::Quat::LookRotation(AXIS_X, AXIS_Z);
	ExpectNear(utl::Rotate(q2, AXIS_Z), AXIS_X);
	ExpectNear(utl::Rotate(q2, AXIS_Y), AXIS_Z);
}

TEST(Quat, LookRotationDegenerateCases) {
	// forward が 0 なら回転なし
	ExpectSameRotation(utl::Quat::LookRotation({ 0.0f, 0.0f, 0.0f }), utl::Quat::Identity());
	// forward と up が平行(真上): testGame と同じく +X は X 軸のまま
	const utl::Quat up = utl::Quat::LookRotation(AXIS_Y);
	ExpectNear(utl::Rotate(up, AXIS_Z), AXIS_Y);
	ExpectNear(utl::Rotate(up, AXIS_X), AXIS_X);
	// up が X 軸に平行なときも壊れない(X に寄せられないので Z 軸を使う)
	const utl::Quat side = utl::Quat::LookRotation(AXIS_X, AXIS_X);
	ExpectNear(utl::Rotate(side, AXIS_Z), AXIS_X);
	EXPECT_NEAR(utl::Dot(utl::Rotate(side, AXIS_X), AXIS_X), 0.0f, 1e-5f);
}

TEST(Quat, NlerpEndsAndShortestPath) {
	const utl::Quat a = utl::Quat::Identity();
	const utl::Quat b = utl::Quat::FromAxisAngle(AXIS_Y, 1.0f);
	ExpectSameRotation(utl::Nlerp(a, b, 0.0f), a);
	ExpectSameRotation(utl::Nlerp(a, b, 1.0f), b);
	// 符号を反転した b でも同じ途中の回転(近い側を通る)
	const utl::Quat negB{ -b.x, -b.y, -b.z, -b.w };
	ExpectSameRotation(utl::Nlerp(a, negB, 0.5f), utl::Nlerp(a, b, 0.5f));
	ExpectSameRotation(utl::Nlerp(a, b, 0.5f), utl::Quat::FromAxisAngle(AXIS_Y, 0.5f), 1e-4f);
}
