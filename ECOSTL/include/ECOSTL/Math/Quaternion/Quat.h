#pragma once
// 回転を表すクォータニオン Quat(POD・確保しない)と、その演算(積・共役・正規化・軸角・ベクトルの回転・向きから作る・補間)。
// testGame の gameLib/Math/MathTypes.h と RigidTransform.h(ベクトルの回転の式)から移植した。
#include <cmath>

#include "ECOSTL/Core/Math/Math.h"
#include "ECOSTL/Math/Vector/Float3.h"

// ecoSTL 全体の名前空間。
namespace utl
{
	/// <summary>
	/// クォータニオン。回転に使うときは単位長(x²+y²+z²+w² = 1)にしておく
	/// </summary>
	struct Quat
	{
		// 四元数の成分(x,y,z が虚部、w が実部)
		float x, y, z, w;
	};

	/// <summary>
	/// 回転なしのクォータニオン
	/// </summary>
	constexpr Quat QuatIdentity() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }

	/// <summary>
	/// 回転の合成。a * b は「b で回してから a で回す」
	/// </summary>
	constexpr Quat operator*(Quat a, Quat b)
	{
		return {
			a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
			a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
			a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
			a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z };
	}

	/// <summary>
	/// 共役。単位クォータニオンなら逆回転になる
	/// </summary>
	constexpr Quat QuatConjugate(Quat q) { return { -q.x, -q.y, -q.z, q.w }; }

	/// <summary>
	/// 内積(2つの回転の近さ。単位クォータニオン同士なら -1..1)
	/// </summary>
	constexpr float QuatDot(Quat a, Quat b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

	/// <summary>
	/// 単位長にする。長さがほぼ 0 のときは割らずにそのまま返す
	/// </summary>
	inline Quat QuatNormalize(Quat q)
	{
		const float l = std::sqrt(QuatDot(q, q));
		const float inv = l > 1e-12f ? 1.0f / l : 1.0f;
		return { q.x * inv, q.y * inv, q.z * inv, q.w * inv };
	}

	/// <summary>
	/// 軸 axis(単位ベクトル)まわりに angle ラジアン回すクォータニオン
	/// </summary>
	inline Quat QuatAxisAngle(Float3 axis, float angle)
	{
		const float h = angle * 0.5f;
		const float s = std::sin(h);
		return { axis.x * s, axis.y * s, axis.z * s, std::cos(h) };
	}

	/// <summary>
	/// ベクトル v を q で回す(q は単位クォータニオン)。q * v * q⁻¹ を展開した式で、行列にしない
	/// </summary>
	constexpr Float3 Rotate(Quat q, Float3 v)
	{
		// t = q.xyz × v + w v、結果 = v + 2 (q.xyz × t)
		const Float3 u{ q.x, q.y, q.z };
		const Float3 t = Cross(u, v) + q.w * v;
		return v + 2.0f * Cross(u, t);
	}

	/// <summary>
	/// 回転行列の3つの列(どれも単位長で互いに直交する右手系の基底)からクォータニオンを作る。
	/// 列はそれぞれローカルの +X・+Y・+Z が回した後に向く方向
	/// </summary>
	inline Quat QuatFromBasis(Float3 xAxis, Float3 yAxis, Float3 zAxis)
	{
		const float m00 = xAxis.x, m01 = yAxis.x, m02 = zAxis.x;
		const float m10 = xAxis.y, m11 = yAxis.y, m12 = zAxis.y;
		const float m20 = xAxis.z, m21 = yAxis.z, m22 = zAxis.z;
		const float tr = m00 + m11 + m22;
		Quat q;
		if (tr > 0.0f)
		{
			const float s = std::sqrt(tr + 1.0f) * 2.0f;
			q = { (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25f * s };
		}
		else if (m00 > m11 && m00 > m22)
		{
			const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
			q = { 0.25f * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s };
		}
		else if (m11 > m22)
		{
			const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
			q = { (m01 + m10) / s, 0.25f * s, (m12 + m21) / s, (m02 - m20) / s };
		}
		else
		{
			const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
			q = { (m02 + m20) / s, (m12 + m21) / s, 0.25f * s, (m10 - m01) / s };
		}
		return QuatNormalize(q);
	}

	/// <summary>
	/// ローカルの +Z を forward に向け、+Y をできるだけ up に寄せる回転。
	/// forward がほぼ 0 なら回転なし。forward と up が平行なら、+X が forward に垂直な向き(X 軸寄り、だめなら Z 軸寄り)になるよう決める
	/// </summary>
	/// <param name="forward">向けたい方向(長さは問わない)</param>
	/// <param name="up">上の目安(長さは問わない。既定は +Y)</param>
	inline Quat QuatLookRotation(Float3 forward, Float3 up = { 0.0f, 1.0f, 0.0f })
	{
		const Float3 f = Normalize(forward);
		if (LengthSq(f) < 0.5f)
		{
			return QuatIdentity();
		}
		Float3 right = Cross(up, f);
		if (LengthSq(right) < 1e-8f)
		{
			// up と forward が平行: forward に垂直な成分を残した軸を右にする
			const Float3 axis = Abs(f.x) < 0.9f ? Float3{ 1.0f, 0.0f, 0.0f } : Float3{ 0.0f, 0.0f, 1.0f };
			right = axis - f * Dot(axis, f);
		}
		right = Normalize(right);
		const Float3 trueUp = Cross(f, right);
		return QuatFromBasis(right, trueUp, f);
	}

	/// <summary>
	/// 正規化した線形補間(nlerp)。t は 0..1。逆向きのクォータニオンは符号をそろえて近い側を通る。
	/// 角速度は一定にならないが、角度が小さいときは slerp とほぼ同じで速い
	/// </summary>
	inline Quat QuatNlerp(Quat a, Quat b, float t)
	{
		const float sgn = QuatDot(a, b) < 0.0f ? -1.0f : 1.0f;
		return QuatNormalize({ Lerp(a.x, b.x * sgn, t), Lerp(a.y, b.y * sgn, t), Lerp(a.z, b.z * sgn, t), Lerp(a.w, b.w * sgn, t) });
	}
}
