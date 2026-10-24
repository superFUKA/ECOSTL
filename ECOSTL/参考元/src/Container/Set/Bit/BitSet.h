#pragma once
#include <cinttypes>
#include <memory>

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


		static_assert(IsPow2(BLOCK_SIZE), "BlockTypeÇÕ2ÇÃôpÇ≈Ç†ÇÈïKóvÇ™Ç†ÇËÇ‹Ç∑ÅB");


	public:

		//-----------------------------------------
		// çÏê¨
		//-----------------------------------------

		constexpr BitSet()
			:bits_()
		{

		}

		//-----------------------------------------
		// ë„ì¸
		//-----------------------------------------


		constexpr BitSet(const BitSet<BIT_NUM, BlockType>& src)
			:bits_()
		{
			*this = src;
		}
		BitSet& operator=(const BitSet<BIT_NUM, BlockType>& src)
		{
			memcpy(&bits_, &src.bits_, sizeof(bits_));
			return *this;
		}

		//-----------------------------------------
		// ëÄçÏ
		//-----------------------------------------


		constexpr void Set(uint32_t index)noexcept
		{
			bits_[index >> BLOCK_SHIFT] |= BlockType(1) << (index & BLOCK_MASK);
		}


		constexpr void Reset(uint32_t index)noexcept
		{
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
		// ÉrÉbÉgââéZ
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
		// éÊìæ
		//-----------------------------------------

		constexpr BlockType GetWord(uint32_t i) const noexcept
		{
			return bits_[i];
		}

		static constexpr uint32_t WordCount() noexcept
		{
			return BLOCK_NUM;
		}

		const BlockType* GetData()const
		{
			return &bits_[0];
		}

		uint32_t GetSize()const
		{
			return BLOCK_NUM * sizeof(BlockType);
		}

		//-----------------------------------------
		// î‰är
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

		[[nodiscard]] constexpr bool Test(size_t index) const noexcept
		{
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
				// ìÒÇ¬ÇÃBitsÇÃã§í Ç™à¯êîÇ∆àÍívÇµÇ»ÇØÇÍÇŒé∏îs
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
