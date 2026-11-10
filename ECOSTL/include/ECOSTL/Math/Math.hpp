#pragma once
// Math 層(Core の上。Core だけに依存する): ベクトル・クォータニオン・行列・剛体の変換・状態を持たない乱数・SIMD のレーン
// 型を選ばない Min/Max・Clamp・Lerp などは Core/Math/Math.h にある
#include "Vector/Float3.h"
#include "Quaternion/Quat.h"
#include "Matrix/Mat4.h"
#include "Transform/RigidTransform.h"
#include "Random/HashRandom.h"
#include "Simd/SimdLanes.h"
