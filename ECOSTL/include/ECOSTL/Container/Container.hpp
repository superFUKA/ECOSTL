#pragma once
// Container 層(Core・Algorithm・Memory の上に作ったコンテナ)をまとめて取り込むヘッダー
//-----------------------------
// ID
//-----------------------------
#include "ID/RecyclableID.h"
//-----------------------------
// Bit
//-----------------------------
#include "Set/Bit/ResizableBitSet.h"
#include "Set/Bit/SparseBitSet.h"
#include "Set/Bit/BitSetHash.h"
#include "Set/Bloom/BloomFilter.h"
//-----------------------------
// Array
//-----------------------------
#include "Array/Static/StaticArray.h"
#include "Array/Dynamic/DynamicArray.h"
#include "Array/Dynamic/DynamicMultiArray.h"
#include "Array/Resizable/ResizableArray.h"
#include "Array/Inline/TrivialInlineArray.h"
#include "Array/View/ArrayView.h"
//-----------------------------
// Vector
//-----------------------------
#include "Vector/Vector.h"
#include "Vector/ChunkVector.h"
//-----------------------------
// String
//-----------------------------
#include "String/String.h"
//-----------------------------
// Set
//-----------------------------
#include "Set/Sorted/SortedVectorSet.h"
#include "Set/Sorted/FixedSortedSet.h"
#include "Set/RobinHoodTable.h"
#include "Set/HashTable.h"
//-----------------------------
// Map
//-----------------------------
#include "Map/DenceMap.h"
#include "Map/ChunkDenceMap.h"
#include "Map/Hash/DenceHashMap.h"
#include "Map/HashMap.h"
