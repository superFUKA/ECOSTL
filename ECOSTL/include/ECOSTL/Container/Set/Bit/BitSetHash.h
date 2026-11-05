#pragma once
// ビットセットの内容から 64bit ハッシュを求める関数群。
#include "BitSet.h"
#include "ResizableBitSet.h"
#include "ECOSTL/Core/Hash/Hash.h"

// 汎用ユーティリティ名前空間
namespace utl
{
	// BitSet のバイト列を seed からつないでハッシュ化する(確保なし)
	template<uint32_t BIT_NUM, typename BlockType = uint64_t>
	Hash64 Hash64FromBitSet(const BitSet<BIT_NUM, BlockType>& src, const Hash64 seed = utl::HASH_OFFSET)
	{
		utl::Hash64 result = seed;

		const uint8_t* begin = reinterpret_cast<const uint8_t*>(src.GetData());
		uint32_t size = src.GetSize();
		result = utl::Hash64FromBytes(begin, static_cast<uint64_t>(size), result);

		return result;
	}

	// ResizableBitSet の各 BitSet を順にハッシュへ畳み込む(確保なし。seed は使われず HASH_OFFSET から始まる)
	template<uint32_t BIT_NUM, typename BlockType = uint64_t>
	Hash64 Hash64FromResizableBitSet(const ResizableBitSet<BIT_NUM, BlockType>& src, const Hash64 seed = utl::HASH_OFFSET)
	{
		utl::Hash64 result = utl::HASH_OFFSET;

		for (uint32_t i = 0; i < src.GetBitSetNum(); i++)
		{
			result = Hash64FromBitSet(src.GetData()[i], result);
		}

		return result;
	}
}