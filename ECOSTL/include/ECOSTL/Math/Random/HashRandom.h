#pragma once
// 状態を持たない乱数。seed を Hash32Mix で混ぜて作るので、同じ seed なら必ず同じ値になり、並列から安全に呼べる。
// 例: 各要素の添え字やフレーム番号を seed にして、要素ごとにばらばらの値を得る。暗号には使えない。
// testGame の gameLib/Math/MathTypes.h(HashU32・Rand01・RandSigned・RandDirection)から移植した。
#include <cmath>
#include <cstdint>

#include "ECOSTL/Core/Hash/Hash.h"
#include "ECOSTL/Core/Math/Math.h"
#include "ECOSTL/Math/Vector/Float3.h"

// ecoSTL 全体の名前空間。
namespace utl
{
	/// <summary>
	/// 0 以上 1 未満の一様な乱数(24bit の精度)
	/// </summary>
	constexpr float Random01(uint32_t seed)
	{
		return static_cast<float>(Hash32Mix(seed) >> 8) * (1.0f / 16777216.0f);
	}

	/// <summary>
	/// -1 以上 1 未満の一様な乱数
	/// </summary>
	constexpr float RandomSigned(uint32_t seed)
	{
		return Random01(seed) * 2.0f - 1.0f;
	}

	/// <summary>
	/// 単位球面の上で一様な方向(長さ 1)。棄却法を使わないので、呼ぶたびの手間が一定
	/// </summary>
	inline Float3 RandomDirection(uint32_t seed)
	{
		const float z = RandomSigned(seed * 3U + 0x9e3779b9U);
		const float a = Random01(seed * 7U + 0x85ebca6bU) * TAU;
		const float r = std::sqrt(Max(0.0f, 1.0f - z * z));
		return { r * std::cos(a), z, r * std::sin(a) };
	}
}
