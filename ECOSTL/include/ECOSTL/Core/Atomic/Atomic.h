#pragma once
// 64bit の原子的な計数（std::atomic の代わり）。コンパイラの組み込み関数で書く。
// 必要な操作（relaxed の加算と読み取り）だけを持つ。順序の保証が要る同期には使わないこと。
#include <cstdint>
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

// 汎用ユーティリティ名前空間
namespace utl
{
	/// <summary>
	/// 複数のスレッドから足してよい 64bit の計数。順序の保証はない（relaxed）
	/// </summary>
	class AtomicCounter64
	{
	public:
		constexpr AtomicCounter64() = default;
		AtomicCounter64(const AtomicCounter64&) = delete;
		AtomicCounter64& operator=(const AtomicCounter64&) = delete;

		/// <summary>value を足し、足す前の値を返す</summary>
		inline uint64_t FetchAdd(const uint64_t value)noexcept
		{
#if defined(_MSC_VER) && !defined(__clang__)
			return static_cast<uint64_t>(_InterlockedExchangeAdd64(reinterpret_cast<volatile long long*>(&value_), static_cast<long long>(value)));
#else
			return __atomic_fetch_add(&value_, value, __ATOMIC_RELAXED);
#endif
		}

		/// <summary>今の値</summary>
		inline uint64_t Load()const noexcept
		{
#if defined(_MSC_VER) && !defined(__clang__)
			// 揃った 64bit の読み取りは x64/ARM64 では原子的。volatile で最適化による読み飛ばしを防ぐ
			return *reinterpret_cast<const volatile uint64_t*>(&value_);
#else
			return __atomic_load_n(&value_, __ATOMIC_RELAXED);
#endif
		}

	private:
		alignas(8) uint64_t value_ = 0;
	};
}
