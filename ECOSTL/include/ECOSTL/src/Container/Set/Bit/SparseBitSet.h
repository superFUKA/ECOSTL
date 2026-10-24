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

		/// <summary>
		/// 標準のメモリアロケータで作成する
		/// </summary>
		SparseBitSet()
			:bitBlocks_()
			, blockIndeces_()
		{}

		/// <summary>
		/// 使用するメモリアロケータを指定して作成する
		/// </summary>
		explicit SparseBitSet(const MemoryAllocatorRef& allocator)
			:bitBlocks_(allocator)
			, blockIndeces_(allocator)
		{}

		/// <summary>
		/// 指定ビットを立てる
		/// </summary>
		inline void Set(const uint32_t index)
		{
			const uint32_t blockIndex = index >> BLOCK_SHIFT;
			const uint32_t bitIndex = index & BLOCK_MASK;

			bool isFind = false;
			uint32_t insertIndex = 0;
			for (insertIndex = 0; insertIndex < blockIndeces_.Size(); ++insertIndex)
			{
				uint32_t lookIndex = blockIndeces_[insertIndex];
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
				// 確保に失敗したら何もしない
				if (newBlock == nullptr)
				{
					return;
				}
				newBlock->Clear();
				newBlock->Set(bitIndex);
				if (blockIndeces_.Insert(insertIndex, blockIndex) == nullptr)
				{
					bitBlocks_.Erase(insertIndex);
				}
			}
		}

		/// <summary>
		/// 指定ビットを落とす
		/// </summary>
		inline void Reset(const uint32_t index)
		{
			const uint32_t blockIndex = index >> BLOCK_SHIFT;
			const uint32_t bitIndex = index & BLOCK_MASK;

			for (uint32_t i = 0; i < blockIndeces_.Size(); ++i)
			{
				uint32_t lookIndex = blockIndeces_[i];
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

		/// <summary>
		/// 全ビットを落とし、ブロックを解放する
		/// </summary>
		inline void Clear()
		{
			bitBlocks_.Clear();
			blockIndeces_.Clear();
		}

		/// <summary>
		/// ビットが1つも立っていないか
		/// </summary>
		inline bool IsEmpty()const
		{
			return bitBlocks_.IsEmpty();
		}

		/// <summary>
		/// 確保しているブロックの数
		/// </summary>
		inline uint32_t BlockCount()const
		{
			return blockIndeces_.Size();
		}

		/// <summary>
		/// 指定ビットが立っているか
		/// </summary>
		inline bool Test(const uint32_t index)const
		{
			const uint32_t blockIndex = index >> BLOCK_SHIFT;
			const uint32_t bitIndex = index & BLOCK_MASK;
			for (uint32_t i = 0; i < blockIndeces_.Size(); ++i)
			{
				uint32_t lookIndex = blockIndeces_[i];
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



		/// <summary>
		/// src のビットをすべて含むか
		/// </summary>
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
				uint32_t lookIndex = blockIndeces_[i];
				uint32_t srcIndex = src.blockIndeces_[j];
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

		/// <summary>
		/// 和集合を代入する
		/// </summary>
		inline void OrAssign(const SparseBitSet& src)
		{
			if (this == &src)
			{
				return;
			}
			uint32_t i = 0;
			uint32_t j = 0;
			while (j < src.blockIndeces_.Size())
			{
				uint32_t srcIndex = src.blockIndeces_[j];
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
					if (!InsertBlock(i, srcIndex, src.bitBlocks_[j]))
					{
						return;
					}
					++i;
					++j;
				}
			}
		}

		/// <summary>
		/// 積集合を代入する
		/// </summary>
		inline void AndAssign(const SparseBitSet& src)
		{
			if (this == &src)
			{
				return;
			}
			uint32_t i = 0;
			uint32_t j = 0;
			while (i < blockIndeces_.Size())
			{
				if (j >= src.blockIndeces_.Size())
				{
					RemoveBlockAt(i);
					continue;
				}
				uint32_t lookIndex = blockIndeces_[i];
				uint32_t srcIndex = src.blockIndeces_[j];
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

		/// <summary>
		/// 排他的論理和を代入する
		/// </summary>
		inline void XorAssign(const SparseBitSet& src)
		{
			// 自分自身との排他的論理和は空になる
			if (this == &src)
			{
				Clear();
				return;
			}
			uint32_t i = 0;
			uint32_t j = 0;
			while (j < src.blockIndeces_.Size())
			{
				uint32_t srcIndex = src.blockIndeces_[j];
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
					if (!InsertBlock(i, srcIndex, src.bitBlocks_[j]))
					{
						return;
					}
					++i;
					++j;
				}
			}
		}

		/// <summary>
		/// 差集合（this から src のビットを落とす）を代入する
		/// </summary>
		inline void AndNotAssign(const SparseBitSet& src)
		{
			// 自分自身との差集合は空になる
			if (this == &src)
			{
				Clear();
				return;
			}
			uint32_t i = 0;
			uint32_t j = 0;
			while (i < blockIndeces_.Size() && j < src.blockIndeces_.Size())
			{
				uint32_t lookIndex = blockIndeces_[i];
				uint32_t srcIndex = src.blockIndeces_[j];
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

		/// <summary>
		/// ブロックの配列を取得する（GetBlockIndeces と同じ並び）
		/// </summary>
		inline const utl::Vector<BitSet<BLOCK_BITS, uint32_t>>& GetBitBlocks() const
		{
			return bitBlocks_;
		}

		/// <summary>
		/// ブロック番号の配列を取得する（昇順）
		/// </summary>
		inline const utl::Vector<uint32_t>& GetBlockIndeces() const
		{
			return blockIndeces_;
		}

	private:

		inline void RemoveBlockAt(const uint32_t removeIndex)
		{
			bitBlocks_.Erase(removeIndex);
			blockIndeces_.Erase(removeIndex);
		}

		/// <summary>
		/// ブロックを挿入する。確保に失敗したら何も変えず false
		/// </summary>
		inline bool InsertBlock(const uint32_t index, const uint32_t blockIndex, const BitSet<BLOCK_BITS, uint32_t>& block)
		{
			if (bitBlocks_.Insert(index, block) == nullptr)
			{
				return false;
			}
			if (blockIndeces_.Insert(index, blockIndex) == nullptr)
			{
				bitBlocks_.Erase(index);
				return false;
			}
			return true;
		}

	private:

		utl::Vector<BitSet<BLOCK_BITS, uint32_t>> bitBlocks_;

		utl::Vector<uint32_t> blockIndeces_;
	};
}
