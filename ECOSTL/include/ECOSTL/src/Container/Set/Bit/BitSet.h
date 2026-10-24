#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>

namespace utl
{

	template<uint32_t BIT_NUM, typename BlockType = uint64_t>
	class BitSet
	{
		template<uint32_t SUB_BIT_NUM, typename SubBlockType>
		friend class BitSet;
		static constexpr bool IsPow2(uint32_t x) noexcept
		{
			return x && ((x & (x - 1)) == 0);
		}
		static constexpr uint32_t Log2Pow2(uint32_t x) noexcept
		{
			uint32_t s = 0;
			while ((x >> s) != 1u) { ++s; }
			return s;
		}
		constexpr inline static uint32_t BLOCK_SIZE = sizeof(BlockType) * 8;
		constexpr inline static uint32_t BLOCK_SHIFT = Log2Pow2(BLOCK_SIZE);
		constexpr inline static uint32_t BLOCK_MASK = BLOCK_SIZE - 1;
		constexpr inline static uint32_t SLACK_SIZE = (BIT_NUM & BLOCK_MASK) ? 1u : 0u;
		constexpr inline static uint32_t BLOCK_NUM = (BIT_NUM >> BLOCK_SHIFT) + SLACK_SIZE;


		static_assert(IsPow2(BLOCK_SIZE), "BlockTypeは2の冪である必要があります。");


	public:

		//-----------------------------------------
		// 作成
		//-----------------------------------------

		constexpr BitSet()
			:bits_()
		{

		}

		//-----------------------------------------
		// 代入
		//-----------------------------------------


		constexpr BitSet(const BitSet<BIT_NUM, BlockType>& src) = default;
		constexpr BitSet& operator=(const BitSet<BIT_NUM, BlockType>& src) = default;

		//-----------------------------------------
		// 操作
		//-----------------------------------------


		constexpr void Set(uint32_t index)noexcept
		{
			assert(index < BIT_NUM && "範囲外のビットです");
			bits_[index >> BLOCK_SHIFT] |= BlockType(1) << (index & BLOCK_MASK);
		}


		constexpr void Reset(uint32_t index)noexcept
		{
			assert(index < BIT_NUM && "範囲外のビットです");
			bits_[index >> BLOCK_SHIFT] &= ~(BlockType(1) << (index & BLOCK_MASK));
		}

		constexpr void Clear()noexcept
		{
			for (uint32_t i = 0; i < BLOCK_NUM; ++i)
			{
				bits_[i] = 0;
			}
		}

		//-----------------------------------------
		// ビット演算
		//-----------------------------------------

		constexpr void OrAssign(const BitSet<BIT_NUM, BlockType>& src) noexcept
		{
			for (uint32_t i = 0; i < BLOCK_NUM; ++i)
			{
				bits_[i] |= src.bits_[i];
			}
		}

		constexpr void AndAssign(const BitSet<BIT_NUM, BlockType>& src) noexcept
		{
			for (uint32_t i = 0; i < BLOCK_NUM; ++i)
			{
				bits_[i] &= src.bits_[i];
			}
		}

		constexpr void XorAssign(const BitSet<BIT_NUM, BlockType>& src) noexcept
		{
			for (uint32_t i = 0; i < BLOCK_NUM; ++i)
			{
				bits_[i] ^= src.bits_[i];
			}
		}

		constexpr void AndNotAssign(const BitSet<BIT_NUM, BlockType>& src) noexcept
		{
			for (uint32_t i = 0; i < BLOCK_NUM; ++i)
			{
				bits_[i] &= ~src.bits_[i];
			}
		}

		//-----------------------------------------
		// 取得
		//-----------------------------------------

		constexpr BlockType GetWord(uint32_t i) const noexcept
		{
			assert(i < BLOCK_NUM && "範囲外のブロックです");
			return bits_[i];
		}

		static constexpr uint32_t WordCount() noexcept
		{
			return BLOCK_NUM;
		}

		const BlockType* GetData()const noexcept
		{
			return &bits_[0];
		}

		uint32_t GetSize()const noexcept
		{
			return BLOCK_NUM * sizeof(BlockType);
		}

		//-----------------------------------------
		// 比較
		//-----------------------------------------

		[[nodiscard]] constexpr bool Empty()const noexcept
		{
			for (uint32_t i = 0; i < BLOCK_NUM; i++)
			{
				if (bits_[i] != 0)
				{
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] constexpr bool Test(uint32_t index) const noexcept
		{
			assert(index < BIT_NUM && "範囲外のビットです");
			return (bits_[index >> BLOCK_SHIFT] >> (index & BLOCK_MASK)) & 1;
		}

		[[nodiscard]] constexpr bool operator==(const BitSet<BIT_NUM, BlockType>& src) const noexcept
		{
			for (uint32_t i = 0; i < BLOCK_NUM; ++i)
			{
				if (bits_[i] != src.bits_[i])
				{
					return false;
				}
			}
			return true;
		}
		[[nodiscard]] constexpr bool operator!=(const BitSet<BIT_NUM, BlockType>& src) const noexcept
		{
			return !(*this == src);
		}

		[[nodiscard]] constexpr bool IsContain(const BitSet<BIT_NUM, BlockType>& src) const noexcept
		{
			for (uint32_t i = 0; i < BLOCK_NUM; ++i)
			{
				// 二つのBitsの共通が引数と一致しなければ失敗
				if ((bits_[i] & src.bits_[i]) != src.bits_[i])
				{
					return false;
				}
			}
			return true;
		}

	private:

		BlockType bits_[BLOCK_NUM];

	};
}
