#pragma once
// 剛体の変換 RigidTransform(位置 + 単位クォータニオンの回転。拡大縮小なし)と、その合成・逆・点の変換。
// 世界 = 親の世界 × ローカル(位置 = 親の位置 + 親の回転 × ローカルの位置、回転 = 親の回転 × ローカルの回転)。
// testGame の gameLib/Math/RigidTransform.h の式を、型を決めた形にして移植した(ECS の部品の型に合わせるテンプレートは gameLib に残す)。
#include "ECOSTL/Math/Quaternion/Quat.h"
#include "ECOSTL/Math/Vector/Float3.h"

// ecoSTL 全体の名前空間。
namespace utl
{
	/// <summary>
	/// 剛体の変換。点 p を rotation で回してから position だけずらす
	/// </summary>
	struct RigidTransform
	{
		// 平行移動
		Float3 position;
		// 回転(単位クォータニオン)
		Quat rotation;
	};

	/// <summary>
	/// 何もしない変換
	/// </summary>
	constexpr RigidTransform RigidTransformIdentity() { return { { 0.0f, 0.0f, 0.0f }, QuatIdentity() }; }

	/// <summary>
	/// 点 p を変換する(rotation で回してから position を足す)
	/// </summary>
	constexpr Float3 TransformPoint(const RigidTransform& t, Float3 p)
	{
		return t.position + Rotate(t.rotation, p);
	}

	/// <summary>
	/// 親の世界の変換とローカルの変換から、世界の変換を作る(世界 = 親 × ローカル)
	/// </summary>
	constexpr RigidTransform Compose(const RigidTransform& parent, const RigidTransform& local)
	{
		return { TransformPoint(parent, local.position), parent.rotation * local.rotation };
	}

	/// <summary>
	/// 逆の変換(Compose(t, Inverse(t)) は何もしない変換になる)
	/// </summary>
	constexpr RigidTransform Inverse(const RigidTransform& t)
	{
		const Quat inv = QuatConjugate(t.rotation);
		return { Rotate(inv, -t.position), inv };
	}
}
