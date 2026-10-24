#pragma once
#include "../../Vector/Vector.h"
#include "BitSet.h"

namespace utl
{
	template<uint32_t BLOCK_BIT_NUM, typename BlockType = uint64_t>
	class ResizableBitSet
	{
	public:

		//-----------------------------------------
		// 作成
		//-----------------------------------------

		ResizableBitSet()
			:bitSets_()
		{

		}

		//-----------------------------------------
		// 代入
		//-----------------------------------------

		ResizableBitSet(const ResizableBitSet& src)
			:bitSets_(src.bitSets_)
		{

		}
		ResizableBitSet& operator=(const ResizableBitSet& src)
		{
			bitSets_ = src.bitSets_;
			return *this;
		}

		ResizableBitSet(ResizableBitSet&& src)noexcept
			:bitSets_(src.bitSets_)
		{

		}
		ResizableBitSet& operator=(ResizableBitSet&& src)noexcept
		{
			bitSets_ = src.bitSets_;
			return *this;
		}

		//-----------------------------------------
		// 操作
		//-----------------------------------------


		constexpr void Set(const uint32_t index)
		{
			const uint32_t blockIndex = index / BLOCK_BIT_NUM;
			const uint32_t bitIndex = index % BLOCK_BIT_NUM;

			if (!bitSets_.IsContain(blockIndex))
			{
				bitSets_.Resize(blockIndex + 1);
			}

			bitSets_[blockIndex].Set(bitIndex);
		}


		constexpr void Reset(uint32_t index)noexcept
		{
			const uint32_t blockIndex = index / BLOCK_BIT_NUM;
			const uint32_t bitIndex = index % BLOCK_BIT_NUM;

			if (!bitSets_.IsContain(blockIndex))
			{
				return;
			}
			bitSets_[blockIndex].Reset(bitIndex);
		}

		constexpr void Clear()noexcept
		{
			bitSets_.Clear();
		}

		//-----------------------------------------
		// 取得
		//-----------------------------------------

		const Vector<BitSet<BLOCK_BIT_NUM, BlockType>>& GetData()const
		{
			return bitSets_;
		}

		uint32_t GetBitSetNum()const
		{
			return bitSets_.Size();
		}

		uint32_t GetBitsNum()const
		{
			uint32_t result = 0;
			for (uint32_t i = 0; i < bitSets_.Size(); i++)
			{
				result += bitSets_[i].GetSize();
			}
		}

		//-----------------------------------------
		// 比較
		//-----------------------------------------


		[[nodiscard]] constexpr bool Test(uint32_t index) const noexcept
		{
			const uint32_t blockIndex = index / BLOCK_BIT_NUM;
			const uint32_t bitIndex = index % BLOCK_BIT_NUM;
			if (!bitSets_.IsContain(blockIndex))
			{
				return false;
			}
			return bitSets_[blockIndex].Test(bitIndex);
		}

		[[nodiscard]] constexpr bool operator==(const ResizableBitSet<BLOCK_BIT_NUM, BlockType>& src) const noexcept
		{
			const uint32_t checkNum = bitSets_.Size() < src.bitSets_.Size() ? bitSets_.Size() : src.bitSets_.Size();

			for (uint32_t i = 0; i < checkNum; ++i)
			{
				if (bitSets_[i] != src.bitSets_[i])
				{
					return false;
				}
			}

			for (uint32_t i = checkNum; i < bitSets_.Size(); ++i)
			{
				if (!bitSets_[i].Empty())
				{
					return false;
				}
			}

			for (uint32_t i = checkNum; i < src.bitSets_.Size(); ++i)
			{
				if (!src.bitSets_[i].Empty())
				{
					return false;
				}
			}
			return true;
		}
		[[nodiscard]] constexpr bool operator!=(const ResizableBitSet<BLOCK_BIT_NUM, BlockType>& src) const noexcept
		{
			return !(*this == src);
		}


		/// <summary>
		/// 引数のビットセットを含むならTrue
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		[[nodiscard]] constexpr bool IsContain(const ResizableBitSet<BLOCK_BIT_NUM, BlockType>& src) const noexcept
		{
			const uint32_t checkNum = bitSets_.Size() < src.bitSets_.Size() ? bitSets_.Size() : src.bitSets_.Size();

			for (uint32_t i = 0; i < checkNum; ++i)
			{
				if (!bitSets_[i].IsContain(src.bitSets_[i]))
				{
					return false;
				}
			}

			for (uint32_t i = checkNum; i < src.bitSets_.Size(); ++i)
			{
				if (!src.bitSets_[i].Empty())
				{
					return false;
				}
			}
			return true;
		}



	private:

		Vector<BitSet<BLOCK_BIT_NUM, BlockType>> bitSets_;

	};

	//uint64_t Hash

}