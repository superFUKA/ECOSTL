// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/Math/Transform/RigidTransform.h"

#include <gtest/gtest.h>

namespace
{
	void ExpectNear(utl::Float3 actual, utl::Float3 expected, float eps = 1e-5f)
	{
		EXPECT_NEAR(actual.x, expected.x, eps);
		EXPECT_NEAR(actual.y, expected.y, eps);
		EXPECT_NEAR(actual.z, expected.z, eps);
	}

	void ExpectSameTransform(const utl::RigidTransform& actual, const utl::RigidTransform& expected, float eps = 1e-5f)
	{
		ExpectNear(actual.position, expected.position, eps);
		const float sgn = utl::QuatDot(actual.rotation, expected.rotation) < 0.0f ? -1.0f : 1.0f;
		EXPECT_NEAR(actual.rotation.x * sgn, expected.rotation.x, eps);
		EXPECT_NEAR(actual.rotation.y * sgn, expected.rotation.y, eps);
		EXPECT_NEAR(actual.rotation.z * sgn, expected.rotation.z, eps);
		EXPECT_NEAR(actual.rotation.w * sgn, expected.rotation.w, eps);
	}

	// testGame の gameLib::ComposeWorld を、ローカルを {px..pz, qx..qw} の形に写したもの(結果の照合用)
	utl::RigidTransform ComposeWorldReference(const utl::RigidTransform& parent, const utl::RigidTransform& local)
	{
		const utl::Float3& pp = parent.position;
		const utl::Quat& pr = parent.rotation;
		const float px = local.position.x, py = local.position.y, pz = local.position.z;
		const float qx = local.rotation.x, qy = local.rotation.y, qz = local.rotation.z, qw = local.rotation.w;
		const float cx = pr.y * pz - pr.z * py, cy = pr.z * px - pr.x * pz, cz = pr.x * py - pr.y * px;
		const float tx = cx + pr.w * px, ty = cy + pr.w * py, tz = cz + pr.w * pz;
		utl::RigidTransform w{};
		w.position.x = pp.x + px + 2.0f * (pr.y * tz - pr.z * ty);
		w.position.y = pp.y + py + 2.0f * (pr.z * tx - pr.x * tz);
		w.position.z = pp.z + pz + 2.0f * (pr.x * ty - pr.y * tx);
		w.rotation.x = pr.w * qx + pr.x * qw + pr.y * qz - pr.z * qy;
		w.rotation.y = pr.w * qy - pr.x * qz + pr.y * qw + pr.z * qx;
		w.rotation.z = pr.w * qz + pr.x * qy - pr.y * qx + pr.z * qw;
		w.rotation.w = pr.w * qw - pr.x * qx - pr.y * qy - pr.z * qz;
		return w;
	}
}

TEST(RigidTransform, IdentityDoesNothing) {
	constexpr utl::RigidTransform i = utl::RigidTransformIdentity();
	constexpr utl::Float3 p = utl::TransformPoint(i, { 1.0f, 2.0f, 3.0f });
	static_assert(p.x == 1.0f && p.y == 2.0f && p.z == 3.0f);
}

TEST(RigidTransform, TransformPointRotatesThenTranslates) {
	const utl::RigidTransform t{ { 10.0f, 0.0f, 0.0f }, utl::QuatAxisAngle({ 0.0f, 0.0f, 1.0f }, utl::PI * 0.5f) };
	ExpectNear(utl::TransformPoint(t, { 1.0f, 0.0f, 0.0f }), { 10.0f, 1.0f, 0.0f });
}

TEST(RigidTransform, ComposeMatchesTestGame) {
	const utl::RigidTransform parent{ { 1.0f, -2.0f, 3.0f }, utl::QuatAxisAngle(utl::Normalize({ 1.0f, 2.0f, -1.0f }), 0.9f) };
	const utl::RigidTransform local{ { -0.5f, 4.0f, 2.0f }, utl::QuatAxisAngle(utl::Normalize({ 0.0f, 1.0f, 3.0f }), -1.3f) };
	ExpectSameTransform(utl::Compose(parent, local), ComposeWorldReference(parent, local));
}

TEST(RigidTransform, ComposeEqualsApplyingInOrder) {
	const utl::RigidTransform parent{ { 0.0f, 5.0f, 0.0f }, utl::QuatAxisAngle({ 0.0f, 1.0f, 0.0f }, 0.4f) };
	const utl::RigidTransform local{ { 1.0f, 0.0f, 2.0f }, utl::QuatAxisAngle({ 1.0f, 0.0f, 0.0f }, 1.1f) };
	const utl::Float3 p{ 0.3f, -0.7f, 1.5f };
	ExpectNear(utl::TransformPoint(utl::Compose(parent, local), p), utl::TransformPoint(parent, utl::TransformPoint(local, p)));
}

TEST(RigidTransform, InverseUndoes) {
	const utl::RigidTransform t{ { 2.0f, -1.0f, 4.0f }, utl::QuatAxisAngle(utl::Normalize({ 1.0f, 1.0f, 1.0f }), 2.0f) };
	ExpectSameTransform(utl::Compose(t, utl::Inverse(t)), utl::RigidTransformIdentity());
	ExpectSameTransform(utl::Compose(utl::Inverse(t), t), utl::RigidTransformIdentity());
	const utl::Float3 p{ 5.0f, 6.0f, 7.0f };
	ExpectNear(utl::TransformPoint(utl::Inverse(t), utl::TransformPoint(t, p)), p, 1e-4f);
}
