#pragma once
// Core 層（最下層）: プラットフォーム差の吸収・ハッシュ・関数オブジェクト・型特性とテンプレート補助・値の組・原子的な計数・メモリの写し・数学
#include "Platform/Platform.h"
#include "Hash/Hash.h"
#include "Function/Function.h"
#include "Template/TypeTraits.h"
#include "Template/Template.h"
#include "Template/Tuple.h"
#include "Atomic/Atomic.h"
#include "Memory/MemCopy.h"
#include "Math/Math.h"
