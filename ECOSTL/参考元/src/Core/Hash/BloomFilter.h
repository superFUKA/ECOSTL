#pragma once
#include "../Math/Math.h"
#include "Hash.h"
#include "../../Container/Set/Bit/BitSet.h"

namespace utl
{
    template<uint32_t BIT_COUNT_LOG2 = 8>
    class BloomFilter
    {
    public:
        static constexpr uint32_t FILTER_SIZE = 1u << BIT_COUNT_LOG2;

    private:
        static constexpr uint32_t FILTER_MASK = FILTER_SIZE - 1;
        static constexpr uint32_t SHIFT = ((sizeof(utl::Hash64) * 8) >> 1) - 1;

        static constexpr uint32_t Masked(utl::Hash64 h) noexcept {
            return static_cast<uint32_t>(h) & FILTER_MASK;
        }
        static constexpr uint32_t MaskedRotate(utl::Hash64 h) noexcept {
            return static_cast<uint32_t>(utl::RotL(h, SHIFT)) & FILTER_MASK;
        }

    public:
        BloomFilter()
            : filter_()
        {
        }

        constexpr void SetSingle(utl::Hash64 hash) noexcept
        {
            filter_.Set(Masked(hash));
        }

        constexpr void SetRotate(utl::Hash64 hash) noexcept
        {
            filter_.Set(MaskedRotate(hash));
        }

        constexpr void Set(utl::Hash64 hash) noexcept
        {
            filter_.Set(Masked(hash));
            filter_.Set(MaskedRotate(hash));
        }

        constexpr bool Test(utl::Hash64 hash) const noexcept
        {
            return filter_.Test(Masked(hash)) && filter_.Test(MaskedRotate(hash));
        }

        constexpr bool IsContain(const BloomFilter& other) const noexcept
        {
            return filter_.IsContain(other.filter_);
        }

        constexpr const utl::BitSet<FILTER_SIZE>& GetFilter() const noexcept
        {
            return filter_;
        }

    private:
        utl::BitSet<FILTER_SIZE> filter_;
    };
}
