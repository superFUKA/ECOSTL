// ECOSTL のクラステンプレートを明示的に実体化して、全てのメンバー関数をコンパイルさせる。
// テンプレートの中の誤り（存在しない関数の呼び出しなど）は、使われるまでエラーにならないため、ここで洗い出す。
#include <gtest/gtest.h>
#include "ECOSTL.hpp"

template class utl::Vector<int>;
template class utl::Vector<utl::String>;
template class utl::ChunkVector<int>;
template class utl::DenceMap<int>;
template class utl::ChunkDenceMap<int>;
template class utl::DenceDirectHashMap<int>;
template class utl::DenceHashMap<uint64_t, int>;
template class utl::DynamicArray<int>;
template class utl::ResizableArray<int>;
template class utl::StaticArray<int, 4>;
template class utl::DynamicMultiArray<int, float>;
template class utl::SortedVectorSet<uint32_t>;
template class utl::FixedSortedSet<uint32_t, 8>;
template class utl::ArrayView<int>;
template struct utl::TrivialInlineArray<uint32_t, 4>;
template class utl::BitSet<64>;
template class utl::ResizableBitSet<64>;
template class utl::UniquePtr<int>;
template class utl::UniquePtr<int[]>;
template class utl::SharedPtr<int>;
template struct utl::Tuple<int, float>;

// 実体化がコンパイルできれば十分。実行時に確かめることは無い
TEST(InstantiateAll, Compiles)
{
	SUCCEED();
}
