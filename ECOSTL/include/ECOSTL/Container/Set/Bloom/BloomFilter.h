#pragma once
// 固定長ビット集合 BitSet 上の簡易ブルームフィルタ。ハッシュ値から2ビットを立て、偽陽性ありで「含まれるか」を高速判定する。
#include "ECOSTL/Core/Math/Math.h"
#include "ECOSTL/Core/Hash/Hash.h"
#include "../Bit/BitSet.h"

// ecoSTL 全体の名前空間。
namespace utl
{
    /// <summary>
    /// 2ビット(元のハッシュと回転後のハッシュ)で判定するブルームフィルタ。確保なし・スレッド非安全。
    /// </summary>
    /// <typeparam name="BIT_COUNT_LOG2">ビット数の log2(既定 8 = 256bit)</typeparam>
    template<uint32_t BIT_COUNT_LOG2 = 8>
    class BloomFilter
    {
    public:
        static constexpr uint32_t FILTER_SIZE = 1u << BIT_COUNT_LOG2;  // フィルタのビット数(2のべき乗)

    private:
        static constexpr uint32_t FILTER_MASK = FILTER_SIZE - 1;  // ハッシュ値の下位ビットを取り出すマスク(FILTER_SIZE は2のべき乗)
        static constexpr uint32_t SHIFT = ((sizeof(utl::Hash64) * 8) >> 1) - 1;  // 2本目のハッシュを作る回転量(Hash64 の半分 - 1 = 31bit)

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

        // 1本目と回転後の2本目の両方のビットを立てる(Test と対で使う)
        constexpr void Set(utl::Hash64 hash) noexcept
        {
            filter_.Set(Masked(hash));
            filter_.Set(MaskedRotate(hash));
        }

        constexpr bool Test(utl::Hash64 hash) const noexcept
        {
            return filter_.Test(Masked(hash)) && filter_.Test(MaskedRotate(hash));
        }

        constexpr bool HasAll(const BloomFilter& other) const noexcept
        {
            return filter_.HasAll(other.filter_);
        }

        constexpr const utl::BitSet<FILTER_SIZE>& GetFilter() const noexcept
        {
            return filter_;
        }

    private:
        utl::BitSet<FILTER_SIZE> filter_;  // 実際のビット列。値の単位はビット
    };
}
