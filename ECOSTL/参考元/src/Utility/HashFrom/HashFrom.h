#pragma once
#include "src/Container/Container.hpp"

namespace utl
{
	template<uint32_t BIT_NUM, typename BlockType = uint64_t>
	Hash64 Hash64FromBitSet(const BitSet<BIT_NUM, BlockType>& src, const Hash64 seed = utl::HASH_OFFSET)
	{
		utl::Hash64 result = seed;

		const uint8_t* begin = reinterpret_cast<const uint8_t*>(src.GetData());
		uint32_t size = src.GetSize();
		result = utl::Hash64FromBytes(begin, size, result);

		return result;
	}

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