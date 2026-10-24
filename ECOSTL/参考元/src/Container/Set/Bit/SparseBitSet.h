#pragma once
#include "../../Vector/Vector.h"
#include "BitSet.h"

namespace utl
{
	/// <summary>
	/// まばらなビットを効率的に管理するビットセット
	/// 連番だけどビットがまばらに立つ場合に有効
	/// </summary>
	class SparseBitSet
	{
		static constexpr uint32_t BLOCK_BITS = 32;
		// log2(32)
		static constexpr uint32_t BLOCK_SHIFT = 5;
		// 0b11111
		static constexpr uint32_t BLOCK_MASK = BLOCK_BITS - 1;

	public:

		SparseBitSet()
			:bitBlocks_()
			, blockIndeces_()
		{
		}

		inline void Set(const uint32_t index)
		{
			const uint16_t blockIndex = index >> BLOCK_SHIFT;
			const uint16_t bitIndex = index & BLOCK_MASK;

			bool isFind = false;
			uint16_t insertIndex = 0;
			for (insertIndex = 0; insertIndex < blockIndeces_.Size(); ++insertIndex)
			{
				uint16_t lookIndex = blockIndeces_[insertIndex];
				// 発見
				if (lookIndex == blockIndex)
				{
					isFind = true;
					break;
				}
				// 超過
				if (lookIndex > blockIndex)
				{
					break;
				}
			}
			// ブロックが見つかったら、ビットを立てる
			if (isFind)
			{
				bitBlocks_[insertIndex].Set(bitIndex);
			}
			// ブロックが見つからなかったら、新規作成してビットを立てる
			else
			{
				BitSet<BLOCK_BITS, uint32_t>* newBlock = bitBlocks_.Emplace(insertIndex);
				newBlock->Clear();
				newBlock->Set(bitIndex);
				blockIndeces_.Insert(insertIndex, static_cast<uint16_t>(blockIndex));
			}
		}

		inline void Reset(const uint32_t index)
		{
			const uint16_t blockIndex = index >> BLOCK_SHIFT;
			const uint16_t bitIndex = index & BLOCK_MASK;

			for (uint32_t i = 0; i < blockIndeces_.Size(); ++i)
			{
				uint16_t lookIndex = blockIndeces_[i];
				// 発見
				if (lookIndex == blockIndex)
				{
					bitBlocks_[i].Reset(bitIndex);
					if (bitBlocks_[i].Empty())
					{
						RemoveBlockAt(i);
					}
					return;
				}
				// 超過
				if (lookIndex > blockIndex)
				{
					return;
				}
			}
		}

		inline void Clear()
		{
			bitBlocks_.Clear();
			blockIndeces_.Clear();
		}

		inline bool Empty()const
		{
			return bitBlocks_.Size() == 0;
		}

		inline uint32_t BlockCount()const
		{
			return blockIndeces_.Size();
		}

		inline bool Test(const uint32_t index)const
		{
			const uint16_t blockIndex = index >> BLOCK_SHIFT;
			const uint16_t bitIndex = index & BLOCK_MASK;
			for (uint16_t i = 0; i < blockIndeces_.Size(); ++i)
			{
				uint16_t lookIndex = blockIndeces_[i];
				// 発見
				if (lookIndex == blockIndex)
				{
					return bitBlocks_[i].Test(bitIndex);
				}
				// 超過
				if (lookIndex > blockIndex)
				{
					break;
				}
			}
			return false;
		}



		inline bool IsContain(const SparseBitSet& src) const
		{
			uint32_t i = 0;
			uint32_t j = 0;
			while (j < src.blockIndeces_.Size())
			{
				if (i >= blockIndeces_.Size())
				{
					return false;
				}
				uint16_t lookIndex = blockIndeces_[i];
				uint16_t srcIndex = src.blockIndeces_[j];
				if (lookIndex < srcIndex)
				{
					++i;
					continue;
				}
				if (lookIndex > srcIndex)
				{
					return false;
				}
				if (!bitBlocks_[i].IsContain(src.bitBlocks_[j]))
				{
					return false;
				}
				++i;
				++j;
			}
			return true;
		}

		inline void OrAssign(const SparseBitSet& src)
		{
			uint32_t i = 0;
			uint32_t j = 0;
			while (j < src.blockIndeces_.Size())
			{
				uint16_t srcIndex = src.blockIndeces_[j];
				while (i < blockIndeces_.Size() && blockIndeces_[i] < srcIndex)
				{
					++i;
				}
				if (i < blockIndeces_.Size() && blockIndeces_[i] == srcIndex)
				{
					bitBlocks_[i].OrAssign(src.bitBlocks_[j]);
					++i;
					++j;
				}
				else
				{
					bitBlocks_.Insert(i, src.bitBlocks_[j]);
					blockIndeces_.Insert(i, srcIndex);
					++i;
					++j;
				}
			}
		}

		inline void AndAssign(const SparseBitSet& src)
		{
			uint32_t i = 0;
			uint32_t j = 0;
			while (i < blockIndeces_.Size())
			{
				if (j >= src.blockIndeces_.Size())
				{
					RemoveBlockAt(i);
					continue;
				}
				uint16_t lookIndex = blockIndeces_[i];
				uint16_t srcIndex = src.blockIndeces_[j];
				if (lookIndex < srcIndex)
				{
					RemoveBlockAt(i);
					continue;
				}
				if (lookIndex > srcIndex)
				{
					++j;
					continue;
				}
				bitBlocks_[i].AndAssign(src.bitBlocks_[j]);
				if (bitBlocks_[i].Empty())
				{
					RemoveBlockAt(i);
					++j;
					continue;
				}
				++i;
				++j;
			}
		}

		inline void XorAssign(const SparseBitSet& src)
		{
			uint32_t i = 0;
			uint32_t j = 0;
			while (j < src.blockIndeces_.Size())
			{
				uint16_t srcIndex = src.blockIndeces_[j];
				while (i < blockIndeces_.Size() && blockIndeces_[i] < srcIndex)
				{
					++i;
				}
				if (i < blockIndeces_.Size() && blockIndeces_[i] == srcIndex)
				{
					bitBlocks_[i].XorAssign(src.bitBlocks_[j]);
					if (bitBlocks_[i].Empty())
					{
						RemoveBlockAt(i);
					}
					else
					{
						++i;
					}
					++j;
				}
				else
				{
					bitBlocks_.Insert(i, src.bitBlocks_[j]);
					blockIndeces_.Insert(i, srcIndex);
					++i;
					++j;
				}
			}
		}

		inline void AndNotAssign(const SparseBitSet& src)
		{
			uint32_t i = 0;
			uint32_t j = 0;
			while (i < blockIndeces_.Size() && j < src.blockIndeces_.Size())
			{
				uint16_t lookIndex = blockIndeces_[i];
				uint16_t srcIndex = src.blockIndeces_[j];
				if (lookIndex < srcIndex)
				{
					++i;
					continue;
				}
				if (lookIndex > srcIndex)
				{
					++j;
					continue;
				}
				bitBlocks_[i].AndNotAssign(src.bitBlocks_[j]);
				if (bitBlocks_[i].Empty())
				{
					RemoveBlockAt(i);
				}
				else
				{
					++i;
				}
				++j;
			}
		}



		//------------------------------------------
		// 比較演算
		//------------------------------------------

		inline bool operator==(const SparseBitSet& src) const
		{
			if (blockIndeces_.Size() != src.blockIndeces_.Size())
			{
				return false;
			}
			for (uint32_t i = 0; i < blockIndeces_.Size(); ++i)
			{
				if (blockIndeces_[i] != src.blockIndeces_[i])
				{
					return false;
				}
				if (bitBlocks_[i] != src.bitBlocks_[i])
				{
					return false;
				}
			}
			return true;
		}

		inline bool operator!=(const SparseBitSet& src) const
		{
			return !(*this == src);
		}

		//------------------------------------------
		// 代入演算
		//------------------------------------------

		// 和集合を代入（this |= src）
		inline SparseBitSet& operator|=(const SparseBitSet& src)
		{
			OrAssign(src);
			return *this;
		}

		// 積集合を代入（this &= src）
		inline SparseBitSet& operator&=(const SparseBitSet& src)
		{
			AndAssign(src);
			return *this;
		}

		// 排他的論理和を代入（this ^= src）
		inline SparseBitSet& operator^=(const SparseBitSet& src)
		{
			XorAssign(src);
			return *this;
		}

		// 差集合を代入（this から src に含まれるビットを落とす）
		inline SparseBitSet& operator-=(const SparseBitSet& src)
		{
			AndNotAssign(src);
			return *this;
		}

		//------------------------------------------
		// 二項演算（非破壊）
		//------------------------------------------

		// 和集合
		inline SparseBitSet operator|(const SparseBitSet& src) const
		{
			SparseBitSet result(*this);
			result |= src;
			return result;
		}

		// 積集合
		inline SparseBitSet operator&(const SparseBitSet& src) const
		{
			SparseBitSet result(*this);
			result &= src;
			return result;
		}

		// 排他的論理和
		inline SparseBitSet operator^(const SparseBitSet& src) const
		{
			SparseBitSet result(*this);
			result ^= src;
			return result;
		}

		// 差集合（this から src に含まれるビットを落とす）
		inline SparseBitSet operator-(const SparseBitSet& src) const
		{
			SparseBitSet result(*this);
			result -= src;
			return result;
		}

		//------------------------------------------
		// 包含関係
		//------------------------------------------

		// this が src を含むか（this は src の上位集合か）
		inline bool operator>=(const SparseBitSet& src) const
		{
			return IsContain(src);
		}

		// this が src に含まれるか（this は src の下位集合か）
		inline bool operator<=(const SparseBitSet& src) const
		{
			return src.IsContain(*this);
		}

		// this が src を厳密に含むか（等しくない上位集合か）
		inline bool operator>(const SparseBitSet& src) const
		{
			return (*this >= src) && (*this != src);
		}

		// this が src に厳密に含まれるか（等しくない下位集合か）
		inline bool operator<(const SparseBitSet& src) const
		{
			return (*this <= src) && (*this != src);
		}

		//------------------------------------------
		// 単一ビット操作のショートカット
		//------------------------------------------

		// 指定ビットを追加（this += index）
		inline SparseBitSet& operator+=(const uint32_t index)
		{
			Set(index);
			return *this;
		}

		// 指定ビットを削除（this -= index）
		// ※ SparseBitSet 同士の operator-= は「差集合」を意味する
		inline SparseBitSet& operator-=(const uint32_t index)
		{
			Reset(index);
			return *this;
		}

		// 指定ビットの取得
		inline bool operator[](const uint32_t index) const
		{
			return Test(index);
		}

		const utl::Vector<BitSet<BLOCK_BITS, uint32_t>> GetBitBlocks() const
		{
			return bitBlocks_;
		}

		const utl::Vector<uint16_t> GetBlockIndeces() const
		{
			return blockIndeces_;
		}

		const uint32_t GetBlockCount() const
		{
			return blockIndeces_.Size();
		}

	private:

		inline void RemoveBlockAt(const uint32_t removeIndex)
		{
			const uint32_t last = bitBlocks_.Size();
			if (last == 0 || removeIndex >= last)
			{
				return;
			}
			for (uint32_t i = removeIndex + 1; i < last; ++i)
			{
				bitBlocks_[i - 1] = bitBlocks_[i];
				blockIndeces_[i - 1] = blockIndeces_[i];
			}
			bitBlocks_.Resize(last - 1);
			blockIndeces_.Resize(last - 1);
		}

	private:

		utl::Vector<BitSet<BLOCK_BITS, uint32_t>> bitBlocks_;

		utl::Vector<uint16_t> blockIndeces_;
	};
}
