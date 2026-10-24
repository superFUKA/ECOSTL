#pragma once
#include <cstdint>
#include "../../Vector/Vector.h"
#include "BitSet.h"

namespace utl
{
	/// <summary>
	/// 必要に応じて大きさが増えるビットセット
	/// BLOCK_BIT_NUM個のビットごとにBitSetを確保する
	/// </summary>
	/// <typeparam name="BLOCK_BIT_NUM">1ブロックのビット数</typeparam>
	/// <typeparam name="BlockType">ブロックの内部表現の型</typeparam>
	template<uint32_t BLOCK_BIT_NUM, typename BlockType = uint64_t>
	class ResizableBitSet
	{
		static_assert(BLOCK_BIT_NUM > 0, "BLOCK_BIT_NUMは1以上である必要があります。");

	public:

		// 作成
		//----------------------

		/// <summary>
		/// 使用するメモリアロケータを指定して作成する
		/// </summary>
		/// <param name="alloc">動的に確保するメモリを提供するアロケータ。指定しなければC++標準の手段で確保する</param>
		explicit ResizableBitSet(const MemoryAllocatorRef& alloc = MemoryAllocatorRef())
			: bitSets_(alloc)
		{
		}

		ResizableBitSet(const ResizableBitSet& src) = default;
		ResizableBitSet& operator=(const ResizableBitSet& src) = default;

		ResizableBitSet(ResizableBitSet&& src)noexcept
			: bitSets_(Move(src.bitSets_))
		{
		}
		ResizableBitSet& operator=(ResizableBitSet&& src)noexcept
		{
			if (this == &src)
			{
				return *this;
			}
			bitSets_ = Move(src.bitSets_);
			return *this;
		}

		// 操作
		//----------------------

		/// <summary>
		/// ビットを立てる
		/// 範囲外ならブロックを増やす。確保できなければ何もしない
		/// </summary>
		/// <param name="index"></param>
		void Set(const uint32_t index)
		{
			const uint32_t blockIndex = index / BLOCK_BIT_NUM;
			const uint32_t bitIndex = index % BLOCK_BIT_NUM;

			if (!bitSets_.IsValidIndex(blockIndex))
			{
				bitSets_.Resize(blockIndex + 1);
				// 確保に失敗していたら範囲外に書き込まない
				if (!bitSets_.IsValidIndex(blockIndex))
				{
					return;
				}
			}

			bitSets_[blockIndex].Set(bitIndex);
		}

		/// <summary>
		/// ビットを下ろす
		/// 範囲外なら何もしない
		/// </summary>
		/// <param name="index"></param>
		void Reset(const uint32_t index)noexcept
		{
			const uint32_t blockIndex = index / BLOCK_BIT_NUM;
			const uint32_t bitIndex = index % BLOCK_BIT_NUM;

			if (!bitSets_.IsValidIndex(blockIndex))
			{
				return;
			}
			bitSets_[blockIndex].Reset(bitIndex);
		}

		/// <summary>
		/// 全てのブロックを破棄する
		/// </summary>
		void Clear()noexcept
		{
			bitSets_.Clear();
		}

		// 取得
		//----------------------

		const Vector<BitSet<BLOCK_BIT_NUM, BlockType>>& GetData()const
		{
			return bitSets_;
		}

		/// <summary>
		/// ブロックの数
		/// </summary>
		uint32_t GetBitSetNum()const
		{
			return bitSets_.Size();
		}

		/// <summary>
		/// 確保済みのビット数
		/// </summary>
		uint32_t GetBitsNum()const
		{
			return bitSets_.Size() * BLOCK_BIT_NUM;
		}

		// 比較
		//----------------------

		/// <summary>
		/// ビットが立っているか
		/// 範囲外ならfalse
		/// </summary>
		/// <param name="index"></param>
		[[nodiscard]] bool Test(const uint32_t index) const noexcept
		{
			const uint32_t blockIndex = index / BLOCK_BIT_NUM;
			const uint32_t bitIndex = index % BLOCK_BIT_NUM;
			if (!bitSets_.IsValidIndex(blockIndex))
			{
				return false;
			}
			return bitSets_[blockIndex].Test(bitIndex);
		}

		/// <summary>
		/// 立っているビットが一つもなければtrue
		/// </summary>
		[[nodiscard]] bool IsEmpty() const noexcept
		{
			for (uint32_t i = 0; i < bitSets_.Size(); ++i)
			{
				if (!bitSets_[i].Empty())
				{
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// 立っているビットが同じならtrue
		/// 末尾の全て0のブロックは無視する
		/// </summary>
		[[nodiscard]] bool operator==(const ResizableBitSet<BLOCK_BIT_NUM, BlockType>& src) const noexcept
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
		[[nodiscard]] bool operator!=(const ResizableBitSet<BLOCK_BIT_NUM, BlockType>& src) const noexcept
		{
			return !(*this == src);
		}

		/// <summary>
		/// 引数のビットセットの立っているビットを全て持つならtrue
		/// </summary>
		/// <param name="src"></param>
		[[nodiscard]] bool IsContain(const ResizableBitSet<BLOCK_BIT_NUM, BlockType>& src) const noexcept
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
}
