#pragma once
// 3次元ベクトル Float3(POD・確保しない)と、その演算(和・差・スカラー倍・内積・外積・長さ・正規化)。
// testGame の gameLib/Math/MathTypes.h から移植した。座標系の決まり(どの軸が上か等)は持たない。外積は右手系の式。
#include <cmath>

// ecoSTL 全体の名前空間。
namespace utl
{
	/// <summary>
	/// 3次元ベクトル。位置・速度・方向に使う POD
	/// </summary>
	struct Float3
	{
		// 各成分
		float x, y, z;
	};

	constexpr Float3 operator+(Float3 a, Float3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	constexpr Float3 operator-(Float3 a, Float3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	constexpr Float3 operator-(Float3 a) { return { -a.x, -a.y, -a.z }; }
	constexpr Float3 operator*(Float3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
	constexpr Float3 operator*(float s, Float3 a) { return { a.x * s, a.y * s, a.z * s }; }
	// s で割る(s = 0 は呼ぶ側で避ける)
	constexpr Float3 operator/(Float3 a, float s) { return { a.x / s, a.y / s, a.z / s }; }
	constexpr Float3& operator+=(Float3& a, Float3 b) { a = a + b; return a; }
	constexpr Float3& operator-=(Float3& a, Float3 b) { a = a - b; return a; }
	constexpr Float3& operator*=(Float3& a, float s) { a = a * s; return a; }
	constexpr Float3& operator/=(Float3& a, float s) { a = a / s; return a; }

	/// <summary>
	/// 内積
	/// </summary>
	constexpr float Dot(Float3 a, Float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

	/// <summary>
	/// 外積(右手系の式。Cross(X, Y) = Z)
	/// </summary>
	constexpr Float3 Cross(Float3 a, Float3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }

	/// <summary>
	/// 長さの2乗(平方根を取らないので、長さの比べ合いに使う)
	/// </summary>
	constexpr float LengthSq(Float3 a) { return Dot(a, a); }

	/// <summary>
	/// 長さ
	/// </summary>
	inline float Length(Float3 a) { return std::sqrt(Dot(a, a)); }

	/// <summary>
	/// 長さ 1 にしたベクトル。長さがほぼ 0 のときはゼロベクトルを返す(NaN を出さない)
	/// </summary>
	inline Float3 Normalize(Float3 a)
	{
		const float l = Length(a);
		return l > 1e-12f ? a * (1.0f / l) : Float3{ 0.0f, 0.0f, 0.0f };
	}
}
