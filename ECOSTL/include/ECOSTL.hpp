#pragma once
// ecoSTL 全体の入口ヘッダ。下の層から順に取り込む
//  Core(標準ライブラリのみ) → Logger → Memory → Algorithm → Container → Concurrency・Utility
#include "ECOSTL/Core/Core.hpp"
#include "ECOSTL/Logger/Logger.hpp"
#include "ECOSTL/Memory/Memory.hpp"
#include "ECOSTL/Algorithm/Algorithm.hpp"
#include "ECOSTL/Container/Container.hpp"
#include "ECOSTL/Concurrency/WorkStage.h"
#include "ECOSTL/Utility/Utility.hpp"
