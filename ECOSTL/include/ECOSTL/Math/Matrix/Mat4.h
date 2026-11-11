#pragma once
// 4x4 行列 Mat4(POD・確保しない)と、その積・点の変換。
// 列優先(m[列*4+行])・列ベクトル(v' = M * v)。座標系や深度の範囲の決まりは関数名に出す(RH = 右手系、01 = 深度 0..1)。
// 作る関数(単位・平行移動・拡大・回転・剛体の変換・ビュー・透視投影)は Mat4 の static 関数にする。
// testGame の gameLib/Math/MathTypes.h から移植し、物を置く行列(平行移動・拡大・回転)を足した。
#include <cmath>
#include <cstdint>

#include "ECOSTL/Math/Quaternion/Quat.h"
#include "ECOSTL/Math/Transform/RigidTransform.h"
#include "ECOSTL/Math/Vector/Float3.h"

// ecoSTL 全体の名前空間。
namespace utl
{
	/// <summary>
	/// 4x4 行列。列優先の16要素(m[列*4+行])で、列ベクトルに左から掛ける
	/// </summary>
	struct Mat4
	{
		// 列優先 16 要素(m[列*4+行])
		float m[16];

		/// <summary>row 行 col 列の要素</summary>
		constexpr float& At(uint32_t row, uint32_t col) { return m[col * 4 + row]; }
		/// <summary>row 行 col 列の要素</summary>
		constexpr float At(uint32_t row, uint32_t col) const { return m[col * 4 + row]; }

		/// <summary>
		/// 単位行列
		/// </summary>
		static constexpr Mat4 Identity()
		{
			Mat4 r{};
			r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
			return r;
		}

		/// <summary>
		/// t だけ動かす行列
		/// </summary>
		static constexpr Mat4 Translation(Float3 t)
		{
			Mat4 r = Identity();
			r.At(0, 3) = t.x;
			r.At(1, 3) = t.y;
			r.At(2, 3) = t.z;
			return r;
		}

		/// <summary>
		/// 軸ごとに s 倍する行列
		/// </summary>
		static constexpr Mat4 Scale(Float3 s)
		{
			Mat4 r = Identity();
			r.At(0, 0) = s.x;
			r.At(1, 1) = s.y;
			r.At(2, 2) = s.z;
			return r;
		}

		/// <summary>
		/// q(単位クォータニオン)で回す行列。Rotate(q, v) と同じ回転
		/// </summary>
		static constexpr Mat4 FromQuat(Quat q)
		{
			const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
			const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
			const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
			Mat4 r = Identity();
			r.At(0, 0) = 1.0f - 2.0f * (yy + zz); r.At(0, 1) = 2.0f * (xy - wz); r.At(0, 2) = 2.0f * (xz + wy);
			r.At(1, 0) = 2.0f * (xy + wz); r.At(1, 1) = 1.0f - 2.0f * (xx + zz); r.At(1, 2) = 2.0f * (yz - wx);
			r.At(2, 0) = 2.0f * (xz - wy); r.At(2, 1) = 2.0f * (yz + wx); r.At(2, 2) = 1.0f - 2.0f * (xx + yy);
			return r;
		}

		/// <summary>
		/// 剛体の変換と同じ行列(回してから動かす = Translation(position) * FromQuat(rotation))
		/// </summary>
		static constexpr Mat4 FromRigidTransform(const RigidTransform& t)
		{
			Mat4 r = FromQuat(t.rotation);
			r.At(0, 3) = t.position.x;
			r.At(1, 3) = t.position.y;
			r.At(2, 3) = t.position.z;
			return r;
		}

		/// <summary>
		/// 右手系のビュー行列(カメラは -Z を向き、+Y が上)。eye から target を見る
		/// </summary>
		/// <param name="eye">カメラの位置</param>
		/// <param name="target">見る点(eye と同じではいけない)</param>
		/// <param name="up">上の目安(視線と平行ではいけない)</param>
		static Mat4 LookAtRH(Float3 eye, Float3 target, Float3 up)
		{
			const Float3 f = Normalize(target - eye);
			const Float3 s = Normalize(Cross(f, up));
			const Float3 u = Cross(s, f);
			Mat4 r = Identity();
			r.At(0, 0) = s.x; r.At(0, 1) = s.y; r.At(0, 2) = s.z; r.At(0, 3) = -Dot(s, eye);
			r.At(1, 0) = u.x; r.At(1, 1) = u.y; r.At(1, 2) = u.z; r.At(1, 3) = -Dot(u, eye);
			r.At(2, 0) = -f.x; r.At(2, 1) = -f.y; r.At(2, 2) = -f.z; r.At(2, 3) = Dot(f, eye);
			return r;
		}

		/// <summary>
		/// 右手系・クリップ空間の深度 0..1 の透視投影行列(D3D12・Vulkan の深度の範囲)。近いほど深度が小さい
		/// </summary>
		/// <param name="fovY">縦の画角(ラジアン)</param>
		/// <param name="aspect">幅 / 高さ</param>
		/// <param name="zn">近い面までの距離(0 より大きい)</param>
		/// <param name="zf">遠い面までの距離(zn より大きい)</param>
		static Mat4 PerspectiveRH01(float fovY, float aspect, float zn, float zf)
		{
			const float t = 1.0f / std::tan(fovY * 0.5f);
			Mat4 r{};
			r.At(0, 0) = t / aspect;
			r.At(1, 1) = t;
			r.At(2, 2) = zf / (zn - zf);
			r.At(2, 3) = zn * zf / (zn - zf);
			r.At(3, 2) = -1.0f;
			return r;
		}
	};

	/// <summary>
	/// 行列の積。a * b は「b を掛けてから a を掛ける」変換
	/// </summary>
	constexpr Mat4 operator*(const Mat4& a, const Mat4& b)
	{
		Mat4 r{};
		for (uint32_t c = 0; c < 4; ++c)
		{
			for (uint32_t rw = 0; rw < 4; ++rw)
			{
				float s = 0.0f;
				for (uint32_t k = 0; k < 4; ++k)
				{
					s += a.At(rw, k) * b.At(k, c);
				}
				r.At(rw, c) = s;
			}
		}
		return r;
	}

	/// <summary>
	/// 点 p(w = 1)を変換する。射影(4行目)は無視するので、アフィン変換の行列に使う
	/// </summary>
	constexpr Float3 TransformPoint(const Mat4& a, Float3 p)
	{
		return {
			a.At(0, 0) * p.x + a.At(0, 1) * p.y + a.At(0, 2) * p.z + a.At(0, 3),
			a.At(1, 0) * p.x + a.At(1, 1) * p.y + a.At(1, 2) * p.z + a.At(1, 3),
			a.At(2, 0) * p.x + a.At(2, 1) * p.y + a.At(2, 2) * p.z + a.At(2, 3) };
	}
}
